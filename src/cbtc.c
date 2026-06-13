/* ============================================================================
 * cbtc.c  -  Cone-Based Topology Control (CBTC) simulator
 *
 *   Project #1: "A cone-based distributed topology-control algorithm for
 *   wireless multi-hop networks", Li, Halpern, Bahl, Wang, Wattenhofer,
 *   IEEE/ACM Trans. Networking 13(1), 2005.
 *
 *   Course: Mobile & Pervasive Computing (Univ. of Thessaly).
 *
 *   What it does:
 *     1. loads a network (node coordinates) from a text file,
 *     2. runs CBTC(alpha) at every node (distributed rule, simulated serially),
 *     3. optionally applies the shrink-back optimization,
 *     4. builds the resulting (symmetric) graph,
 *     5. checks connectivity and prints metrics,
 *     6. optionally exports the surviving edges for plotting.
 *
 *   Build:   gcc -Wall -O2 cbtc.c -o cbtc -lm
 *   Usage:   ./cbtc <network_file> [--alpha DEG] [--shrink] [--edges OUT.txt]
 *
 *   Network file format (lines starting with '#' are comments):
 *       N  Rmax
 *       x0 y0
 *       x1 y1
 *       ...
 * ==========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>            /* mkdir() for the edges/ output folder */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAXN 512                 /* max number of nodes */

typedef struct {
    double x, y;                 /* position in the plane                 */
    double radius;               /* current transmission radius (~ power) */
    int    nbr[MAXN];            /* indices of neighbours                 */
    double nbr_angle[MAXN];      /* direction (angle) to each neighbour   */
    int    nbr_count;
} Node;

static Node   nodes[MAXN];
static int    N;                 /* number of nodes            */
static double Rmax;              /* maximum transmission range */
static int    adj[MAXN][MAXN];   /* adjacency matrix of result */

/* ------------------------------------------------------------------ geometry */

static double dist(int a, int b)
{
    double dx = nodes[a].x - nodes[b].x;
    double dy = nodes[a].y - nodes[b].y;
    return sqrt(dx * dx + dy * dy);
}

/* direction from u to v, normalised to [0, 2*pi) */
static double angle_to(int u, int v)
{
    double a = atan2(nodes[v].y - nodes[u].y, nodes[v].x - nodes[u].x);
    if (a < 0.0) a += 2.0 * M_PI;
    return a;
}

static int cmp_double(const void *p, const void *q)
{
    double a = *(const double *)p, b = *(const double *)q;
    return (a > b) - (a < b);
}

/* --------------------------------------------------------------- alpha-gap

   Returns 1 if there is an "alpha-gap": a cone of angle >= alpha around the
   node that contains NO neighbour.  This is the heart of CBTC.
   `angles` is a scratch buffer that WILL be sorted in place.
*/
static int has_alpha_gap(double *angles, int n, double alpha)
{
    int i;
    double maxgap, wrap;

    if (n == 0) return 1;        /* no neighbour at all     -> gap */
    if (n == 1) return 1;        /* one neighbour -> 2*pi gap-> gap */

    qsort(angles, n, sizeof(double), cmp_double);

    maxgap = 0.0;
    for (i = 1; i < n; i++) {
        double g = angles[i] - angles[i - 1];
        if (g > maxgap) maxgap = g;
    }
    /* wrap-around gap, from the last neighbour back to the first */
    wrap = (2.0 * M_PI) - (angles[n - 1] - angles[0]);
    if (wrap > maxgap) maxgap = wrap;

    return maxgap > alpha;
}

/* --------------------------------------------------------------- CBTC core

   Candidate neighbour, used to add nodes in order of increasing distance.
*/
typedef struct { int id; double d; } Cand;

static int cmp_cand(const void *p, const void *q)
{
    double a = ((const Cand *)p)->d, b = ((const Cand *)q)->d;
    return (a > b) - (a < b);
}

/* Run CBTC(alpha) at node u: raise power (radius) until no alpha-gap remains
   or the maximum range Rmax is reached. */
static void run_cbtc(int u, double alpha)
{
    Cand   c[MAXN];
    double angles[MAXN];
    int    m = 0, k, i, v;

    /* 1) collect & sort all candidates within Rmax, by distance */
    for (v = 0; v < N; v++) {
        if (v == u) continue;
        {
            double d = dist(u, v);
            if (d <= Rmax) { c[m].id = v; c[m].d = d; m++; }
        }
    }
    qsort(c, m, sizeof(Cand), cmp_cand);

    /* 2) add neighbours one by one until the cone condition is satisfied */
    nodes[u].nbr_count = 0;
    nodes[u].radius    = 0.0;

    for (k = 0; k < m; k++) {
        v = c[k].id;
        nodes[u].nbr[nodes[u].nbr_count]       = v;
        nodes[u].nbr_angle[nodes[u].nbr_count] = angle_to(u, v);
        nodes[u].nbr_count++;
        nodes[u].radius = c[k].d;                 /* power reaches this far */

        for (i = 0; i < nodes[u].nbr_count; i++)  /* copy (gap test sorts) */
            angles[i] = nodes[u].nbr_angle[i];

        if (!has_alpha_gap(angles, nodes[u].nbr_count, alpha))
            break;                                /* cones covered -> stop */
    }
    /* if the loop ran out without breaking, u is a boundary node at Rmax */
}

/* ------------------------------------------------------------- shrink-back
   (CBTC paper, Section III-A "The shrink-back operation", Theorem III.1)

   A node u is a BOUNDARY node if, at the end of the growing phase, it still
   has an alpha-gap (so it broadcasts at maximum power).  The shrink-back phase
   lets such a node lower its power, *as long as it does not reduce its cone
   coverage*.  Faithfully to the paper: every neighbour is tagged with the
   power (here: distance) at which it was first discovered, and we look for the
   SMALLEST power level (= shortest prefix, since neighbours are stored by
   increasing distance) whose coverage equals the coverage at maximum power:

        find the minimum j  such that  cover_alpha(dir_j) == cover_alpha(dir_k)

   Only neighbours that provide *redundant* cone coverage are dropped, which is
   exactly why connectivity is preserved (Theorem III.1).
*/

/* angular resolution used to represent cover_alpha (0.1 degrees per bin) */
#define COVER_BINS 3600

/* cov[b] = 1 iff the centre angle of bin b lies within alpha/2 of some
   neighbour direction in dirs[0..n-1] (i.e. that direction is covered). */
static void cover_alpha(const double *dirs, int n, double alpha, char *cov)
{
    double h = alpha / 2.0;
    int b, i;
    for (b = 0; b < COVER_BINS; b++) {
        double ang = (b + 0.5) * (2.0 * M_PI / COVER_BINS);
        cov[b] = 0;
        for (i = 0; i < n; i++) {
            double d = fabs(ang - dirs[i]);
            if (d > M_PI) d = 2.0 * M_PI - d;      /* shortest angular distance */
            if (d <= h) { cov[b] = 1; break; }
        }
    }
}

static void shrink_back(int u, double alpha)
{
    int    n = nodes[u].nbr_count;
    double dirs[MAXN];
    char   full[COVER_BINS], pref[COVER_BINS];
    int    i, j, b;

    if (n <= 1) return;                     /* nothing to shrink */

    for (i = 0; i < n; i++) dirs[i] = nodes[u].nbr_angle[i];

    /* coverage at maximum power (all discovered neighbours) */
    cover_alpha(dirs, n, alpha, full);

    /* smallest prefix (lowest power) whose coverage equals the full coverage.
       Neighbours are stored by increasing distance, so a prefix corresponds to
       a lower power level; coverage grows monotonically with the prefix, so the
       first prefix that matches `full` is the minimum-power one. */
    for (j = 1; j <= n; j++) {
        int equal = 1;
        cover_alpha(dirs, j, alpha, pref);
        for (b = 0; b < COVER_BINS; b++)
            if (pref[b] != full[b]) { equal = 0; break; }
        if (equal) break;
    }

    /* keep the first j neighbours, drop the redundant farther ones */
    nodes[u].nbr_count = j;
    nodes[u].radius = dist(u, nodes[u].nbr[j - 1]);
}

/* --------------------------------------------------------------- graph + bfs */

static void build_graph(void)
{
    int u, k;
    for (u = 0; u < N; u++)
        memset(adj[u], 0, sizeof(int) * N);

    /* symmetric (undirected) edges: keep (u,v) if EITHER side selected it */
    for (u = 0; u < N; u++)
        for (k = 0; k < nodes[u].nbr_count; k++) {
            int v = nodes[u].nbr[k];
            adj[u][v] = adj[v][u] = 1;
        }
}

/* number of connected components (1 == connected) */
static int components(void)
{
    int visited[MAXN], stack[MAXN];
    int i, comp = 0;

    for (i = 0; i < N; i++) visited[i] = 0;

    for (i = 0; i < N; i++) {
        if (visited[i]) continue;
        comp++;
        {
            int top = 0;
            visited[i] = 1; stack[top++] = i;
            while (top > 0) {
                int u = stack[--top], v;
                for (v = 0; v < N; v++)
                    if (adj[u][v] && !visited[v]) {
                        visited[v] = 1; stack[top++] = v;
                    }
            }
        }
    }
    return comp;
}

/* --------------------------------------------------------------- file input */

static int load_network(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    int  got_header = 0, i = 0;

    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return 0; }

    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;

        if (!got_header) {
            if (sscanf(p, "%d %lf", &N, &Rmax) != 2) {
                fprintf(stderr, "Bad header in %s\n", path); fclose(f); return 0;
            }
            if (N > MAXN) { fprintf(stderr, "Too many nodes (max %d)\n", MAXN);
                            fclose(f); return 0; }
            got_header = 1;
        } else {
            if (i < N && sscanf(p, "%lf %lf", &nodes[i].x, &nodes[i].y) == 2)
                i++;
        }
    }
    fclose(f);
    if (i != N) { fprintf(stderr, "Expected %d nodes, read %d\n", N, i); return 0; }
    return 1;
}

/* --------------------------------------------------------------------- main */

int main(int argc, char **argv)
{
    double alpha_deg = 150.0, alpha;
    int    use_shrink = 0, u;
    const char *netfile = NULL, *edgefile = NULL;
    int    a;

    for (a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "--alpha") && a + 1 < argc)      alpha_deg = atof(argv[++a]);
        else if (!strcmp(argv[a], "--shrink"))                use_shrink = 1;
        else if (!strcmp(argv[a], "--edges") && a + 1 < argc) edgefile = argv[++a];
        else if (argv[a][0] != '-')                           netfile = argv[a];
    }
    if (!netfile) {
        fprintf(stderr,
            "Usage: %s <network_file> [--alpha DEG] [--shrink] [--edges OUT.txt]\n",
            argv[0]);
        return 1;
    }
    if (!load_network(netfile)) return 1;

    alpha = alpha_deg * M_PI / 180.0;

    /* run CBTC at every node, then optional shrink-back */
    for (u = 0; u < N; u++) run_cbtc(u, alpha);
    if (use_shrink)
        for (u = 0; u < N; u++) shrink_back(u, alpha);

    build_graph();

    /* ---- metrics ---- */
    {
        long   deg_sum = 0;
        double rmax_used = 0.0, energy = 0.0;   /* energy ~ sum of radius^2 */
        int    comp, maxdeg = 0, mindeg = N;

        for (u = 0; u < N; u++) {
            int d = 0, v;
            for (v = 0; v < N; v++) d += adj[u][v];
            deg_sum += d;
            if (d > maxdeg) maxdeg = d;
            if (d < mindeg) mindeg = d;
            if (nodes[u].radius > rmax_used) rmax_used = nodes[u].radius;
            energy += nodes[u].radius * nodes[u].radius;
        }
        comp = components();

        printf("network            : %s\n", netfile);
        printf("nodes / Rmax       : %d / %.2f\n", N, Rmax);
        printf("alpha              : %.1f deg  (%s)\n", alpha_deg,
               alpha_deg <= 150.0 ? "<= 5pi/6, connectivity guaranteed"
                                  : "> 5pi/6, connectivity NOT guaranteed");
        printf("shrink-back        : %s\n", use_shrink ? "ON" : "off");
        printf("avg node degree    : %.2f\n", (double)deg_sum / N);
        printf("min / max degree   : %d / %d\n", mindeg, maxdeg);
        printf("max radius used    : %.2f  (of Rmax %.2f)\n", rmax_used, Rmax);
        printf("total energy (Sr^2): %.1f\n", energy);
        printf("components         : %d  -> %s\n", comp,
               comp == 1 ? "CONNECTED" : "DISCONNECTED");
    }

    /* ---- optional edge export for plotting (saved inside edges/) ---- */
    if (edgefile) {
        char path[600];
        /* if the user gave a bare file name, put it under edges/ ; if they
           gave a path with a '/', respect it as-is */
        if (strchr(edgefile, '/') == NULL) {
            mkdir("edges", 0777);                 /* create edges/ if missing */
            snprintf(path, sizeof path, "edges/%s", edgefile);
        } else {
            snprintf(path, sizeof path, "%s", edgefile);
        }
        FILE *fe = fopen(path, "w");
        if (fe) {
            int v;
            fprintf(fe, "# u v x_u y_u x_v y_v\n");
            for (u = 0; u < N; u++)
                for (v = u + 1; v < N; v++)
                    if (adj[u][v])
                        fprintf(fe, "%d %d %.3f %.3f %.3f %.3f\n",
                                u, v, nodes[u].x, nodes[u].y,
                                nodes[v].x, nodes[v].y);
            fclose(fe);
            fprintf(stderr, "edges written to %s\n", path);
        }
    }
    return 0;
}
