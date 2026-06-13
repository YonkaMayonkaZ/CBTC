# Οδηγός CBTC — Εκτέλεση & Επεξήγηση Κώδικα (γραμμή προς γραμμή)

Αναλυτικός οδηγός για το project **Cone-Based Topology Control (CBTC)**, με αναφορές
στα δύο βασικά κείμενα:

- **[Paper]** L. Li, J. Y. Halpern, P. Bahl, Y.-M. Wang, R. Wattenhofer, *"A cone-based
  distributed topology-control algorithm for wireless multi-hop networks"*, **IEEE/ACM
  Transactions on Networking, vol. 13, no. 1, pp. 147–159, 2005**.
- **[Βιβλίο]** H. Karl, A. Willig, *"Protocols and Architectures for Wireless Sensor
  Networks"*, **Κεφάλαιο 10 — Topology Control** (ιδίως §10.1, §10.2.2, §10.2.3).

---

## Μέρος Α — Πώς τρέχεις τα scripts

### Α.1 Προαπαιτούμενα

- Μεταγλωττιστής **gcc** και η βιβλιοθήκη μαθηματικών (`-lm`).
- Για τη γραφική απεικόνιση: **Python 3** με **matplotlib** (`pip install matplotlib`).

### Α.2 Μεταγλώττιση (build)

Από τον φάκελο `CBTC_Project/`, μεταγλώττισε απευθείας με `gcc`:

```bash
gcc -Wall -Wextra -O2 src/cbtc.c -o cbtc -lm
```

- `-Wall -Wextra` → ενεργοποιούν όλες τις προειδοποιήσεις (καλή πρακτική· αν εμφανιστεί
  warning, κάτι παίζει).
- `-O2` → βελτιστοποίηση ταχύτητας.
- `-lm` → συνδέει τη `libm` για τις `sqrt`, `atan2`, `M_PI`.

Παράγεται το εκτελέσιμο `./cbtc`. Για καθάρισμα των παραγόμενων αρχείων:
`rm -f cbtc *.edges *.png`.

### Α.3 Βασική εκτέλεση

```bash
./cbtc <αρχείο_δικτύου> [--alpha ΜΟΙΡΕΣ] [--shrink] [--edges ΕΞΟΔΟΣ.txt]
```

| Όρισμα | Σημασία | Προεπιλογή |
|--------|---------|------------|
| `<αρχείο_δικτύου>` | το δίκτυο εισόδου (π.χ. `networks/grid.txt`) | — |
| `--alpha ΜΟΙΡΕΣ` | η γωνία κώνου α σε μοίρες | **150** (= 5π/6) |
| `--shrink` | ενεργοποιεί τη βελτιστοποίηση shrink-back | ανενεργό |
| `--edges ΕΞΟΔΟΣ.txt` | εξάγει τις ακμές για γραφική απεικόνιση | — |

Παραδείγματα:

```bash
./cbtc networks/grid.txt                       # τρέχει με α = 150°
./cbtc networks/random100.txt --shrink         # με shrink-back
./cbtc networks/two_clusters.txt --alpha 180   # δοκιμή πάνω από το κατώφλι
./cbtc networks/grid.txt --edges grid.edges    # + εξαγωγή ακμών
```

Η έξοδος τυπώνει: πλήθος κόμβων/Rmax, την α, αν είναι ON το shrink-back, **μέσο βαθμό
κόμβου**, ελάχιστο/μέγιστο βαθμό, **μέγιστη ισχύ (ακτίνα) που χρησιμοποιήθηκε**, **συνολική
ενέργεια** (Σ ακτίνα²) και πλήθος **συνεκτικών συνιστωσών** (1 = συνεκτικό).

### Α.4 Εκτέλεση όλων μαζί

Με έναν βρόχο του φλοιού (bash) τρέχεις τον CBTC σε όλα τα δίκτυα του `networks/`:

```bash
for net in networks/*.txt; do
    echo "=== $net ==="
    ./cbtc "$net" --alpha 150
done
```

Για shrink-back, πρόσθεσε `--shrink` στην κλήση: `./cbtc "$net" --alpha 150 --shrink`.

### Α.5 Γραφική απεικόνιση (πριν / μετά)

Δύο βήματα — πρώτα παράγεις τις ακμές, μετά τις ζωγραφίζεις:

```bash
./cbtc networks/random100.txt --edges random100.edges
python3 plot_topology.py networks/random100.txt random100.edges out.png
```

Το `plot_topology.py` βγάζει εικόνα με **δύο πάνελ**: αριστερά το γράφημα πλήρους ισχύος
(όλα τα ζεύγη εντός Rmax) και δεξιά το γράφημα μετά τον CBTC, με τον αριθμό ακμών και τον
μέσο βαθμό στον τίτλο.

---

## Μέρος Β — Το θεωρητικό πλαίσιο (paper + βιβλίο)

Πριν τον κώδικα, η ιδέα σε μία παράγραφο, ώστε οι αναφορές παρακάτω να βγάζουν νόημα.

Στα ad hoc δίκτυα κάθε κόμβος ρυθμίζει την **ισχύ εκπομπής** του. Στόχος του topology
control: μικρότερη ισχύ/λιγότεροι γείτονες, **χωρίς να χαθεί η συνεκτικότητα** (Βιβλίο
§10.1). Ο CBTC είναι, σύμφωνα με το paper, ένας **κατανεμημένος, δύο φάσεων** αλγόριθμος
που χρειάζεται **μόνο πληροφορία κατεύθυνσης** (angle-of-arrival), όχι GPS/αποστάσεις
(Paper, Ενότητα I–II· Βιβλίο §10.2.3, υποενότητα *"Cone-based topology control"*):

- **Φάση 1 (βασική):** ο κόμβος *u* αυξάνει την ισχύ του μέχρι, **σε κάθε κώνο γωνίας α**
  γύρω του, να υπάρχει τουλάχιστον ένας γείτονας. Το **θεμελιώδες θεώρημα** του paper:
  με **α ≤ 5π/6 (=150°)** η συνεκτικότητα διατηρείται, και το 5π/6 είναι το **ακριβές
  (tight)** όριο.
- **Φάση 2 (βελτιστοποιήσεις):** shrink-back, asymmetric edge removal, pairwise edge
  removal — αφαιρούν περιττή ισχύ/ακμές χωρίς να χαλούν τη συνεκτικότητα.

Το βιβλίο τοποθετεί τον CBTC στην οικογένεια του **flat power-control** (§10.2), δίπλα στα
γεωμετρικά RNG, Gabriel Graph και LMST (§10.2.3), και συζητά στο §10.2.2 ότι **δεν υπάρχει
«μαγικός αριθμός» γειτόνων** που να εγγυάται συνεκτικότητα — γι' αυτό μετράμε εμπειρικά τον
μέσο βαθμό.

---

## Μέρος Γ — Επεξήγηση του `src/cbtc.c` γραμμή προς γραμμή

### Γ.1 Επικεφαλίδες και σταθερές

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAXN 512
```

- `stdio.h` → `printf`, `fopen`, `fgets`. `stdlib.h` → `qsort`, `atof`, `malloc`-free
  κώδικας. `string.h` → `strcmp`, `memset`. `math.h` → `sqrt`, `atan2`.
- `#ifndef M_PI … #endif` → ορισμός του π αν δεν το δίνει ο compiler (μερικά πρότυπα C δεν
  το έχουν εξ ορισμού). Το π το χρειαζόμαστε επειδή οι γωνίες μετρώνται σε ακτίνια — η α
  στο paper εκφράζεται ως κλάσμα του π (π.χ. 5π/6).
- `#define MAXN 512` → άνω όριο κόμβων· καθορίζει το μέγεθος των στατικών πινάκων.

### Γ.2 Η δομή `Node`

```c
typedef struct {
    double x, y;                 // θέση στο επίπεδο
    double radius;               // τρέχουσα ακτίνα εκπομπής (~ ισχύς)
    int    nbr[MAXN];            // δείκτες γειτόνων
    double nbr_angle[MAXN];      // γωνία προς κάθε γείτονα
    int    nbr_count;
} Node;
```

- `x, y` → οι συντεταγμένες. **Προσοχή στη φιλοσοφία:** ο πραγματικός CBTC δεν ξέρει
  συντεταγμένες· εμείς τις κρατάμε **μόνο ως προσομοιωτές**, για να *υπολογίσουμε* τις
  γωνίες και τις αποστάσεις που στην πραγματικότητα θα μετρούσε το υλικό (Paper: χρειάζεται
  μόνο κατεύθυνση).
- `radius` → η ακτίνα στην οποία έχει φτάσει η ισχύς του κόμβου. Είναι το **proxy της
  ισχύος**: μεγαλύτερη ακτίνα = μεγαλύτερη ισχύς (η ενέργεια ∝ ακτίνα^β).
- `nbr[]`, `nbr_angle[]`, `nbr_count` → η λίστα γειτόνων με τις κατευθύνσεις τους. Οι
  **γωνίες** είναι το μόνο που χρησιμοποιεί ουσιαστικά ο αλγόριθμος (έλεγχος κώνων).

```c
static Node   nodes[MAXN];
static int    N;
static double Rmax;
static int    adj[MAXN][MAXN];
```

- `nodes[]` → όλοι οι κόμβοι. `N` → πόσοι. `Rmax` → η **μέγιστη** ακτίνα/ισχύς (το όριο
  του υλικού· αντιστοιχεί στο γράφημα μέγιστης ισχύος **G** του paper). `adj[][]` → πίνακας
  γειτνίασης του τελικού γραφήματος.

### Γ.3 Γεωμετρία: `dist` και `angle_to`

```c
static double dist(int a, int b) {
    double dx = nodes[a].x - nodes[b].x;
    double dy = nodes[a].y - nodes[b].y;
    return sqrt(dx * dx + dy * dy);
}
```

- Ευκλείδεια απόσταση. Τη χρειαζόμαστε για να ξέρουμε **ποιοι κόμβοι είναι προσβάσιμοι** σε
  δεδομένη ακτίνα και για το μοντέλο ενέργειας.

```c
static double angle_to(int u, int v) {
    double a = atan2(nodes[v].y - nodes[u].y, nodes[v].x - nodes[u].x);
    if (a < 0.0) a += 2.0 * M_PI;
    return a;
}
```

- `atan2(dy, dx)` → η **κατεύθυνση** από τον *u* προς τον *v*, στο διάστημα (−π, π].
- `if (a < 0) a += 2π` → **κανονικοποίηση στο [0, 2π)**. Είναι κρίσιμο: ο έλεγχος κώνου
  μετράει γωνιακά κενά «γύρω από τον κύκλο», και αν αφήναμε αρνητικές γωνίες το «τύλιγμα»
  (wrap-around) θα έβγαινε λάθος. Αυτή η γωνία είναι **ακριβώς η πληροφορία AoA** που
  προϋποθέτει το paper.

### Γ.4 Συγκριτής για ταξινόμηση γωνιών

```c
static int cmp_double(const void *p, const void *q) {
    double a = *(const double *)p, b = *(const double *)q;
    return (a > b) - (a < b);
}
```

- Συγκριτής για την `qsort` που ταξινομεί `double`. Το κόλπο `(a>b)-(a<b)` επιστρέφει
  −1/0/1 χωρίς προβλήματα υπερχείλισης. Χρειάζεται για να βάλουμε τις γωνίες σε σειρά πριν
  βρούμε το μέγιστο κενό.

### Γ.5 Η ΚΑΡΔΙΑ — `has_alpha_gap`

Αυτή η συνάρτηση υλοποιεί **τη συνθήκη κώνου του paper**: «σε κάθε κώνο γωνίας α υπάρχει
γείτονας». Ισοδύναμα: **δεν υπάρχει κενός κώνος γωνίας α** ⟺ το μέγιστο γωνιακό κενό μεταξύ
διαδοχικών γειτόνων είναι ≤ α.

```c
static int has_alpha_gap(double *angles, int n, double alpha) {
    int i;
    double maxgap, wrap;

    if (n == 0) return 1;        // κανένας γείτονας -> κενό
    if (n == 1) return 1;        // ένας γείτονας -> κενό 2π -> κενό
```

- Επιστρέφει `1` αν **υπάρχει α-gap** (κενός κώνος ≥ α), αλλιώς `0`.
- Οριακές περιπτώσεις: με 0 ή 1 γείτονα υπάρχει σίγουρα τεράστιο κενό (2π), άρα `1`. (Αυτό
  εξηγεί γιατί τα **φύλλα του αστέρα** και τα **άκρα της αλυσίδας** φτάνουν πάντα στη μέγιστη
  ισχύ.)

```c
    qsort(angles, n, sizeof(double), cmp_double);

    maxgap = 0.0;
    for (i = 1; i < n; i++) {
        double g = angles[i] - angles[i - 1];
        if (g > maxgap) maxgap = g;
    }
```

- Ταξινομούμε τις γωνίες κυκλικά και βρίσκουμε το **μεγαλύτερο κενό** ανάμεσα σε δύο
  **διαδοχικούς** γείτονες.

```c
    wrap = (2.0 * M_PI) - (angles[n - 1] - angles[0]);
    if (wrap > maxgap) maxgap = wrap;

    return maxgap > alpha;
}
```

- `wrap` → το κενό που «τυλίγεται» από τον τελευταίο γείτονα (μεγαλύτερη γωνία) πίσω στον
  πρώτο (μικρότερη). Χωρίς αυτό θα χάναμε το κενό που περνά πάνω από το 0°/360°.
- `return maxgap > alpha` → αν το μέγιστο κενό ξεπερνά την α, **χωράει** κενός κώνος γωνίας
  α → υπάρχει α-gap. Αυτή η μία γραμμή είναι η μαθηματική καρδιά του CBTC (Paper, βασική
  συνθήκη της Φάσης 1· Βιβλίο §10.2.3).

### Γ.6 Υποψήφιοι γείτονες — `Cand`, `cmp_cand`

```c
typedef struct { int id; double d; } Cand;

static int cmp_cand(const void *p, const void *q) {
    double a = ((const Cand *)p)->d, b = ((const Cand *)q)->d;
    return (a > b) - (a < b);
}
```

- Βοηθητική δομή: ένας υποψήφιος γείτονας με την **απόστασή** του. Ο συγκριτής τους
  ταξινομεί κατά **αύξουσα απόσταση**, ώστε στο επόμενο βήμα να τους προσθέτουμε «από τον
  κοντινότερο προς τον μακρινότερο» — δηλαδή να αυξάνουμε σταδιακά την ισχύ.

### Γ.7 Ο βασικός αλγόριθμος — `run_cbtc` (Φάση 1 του paper)

```c
static void run_cbtc(int u, double alpha) {
    Cand   c[MAXN];
    double angles[MAXN];
    int    m = 0, k, i, v;

    for (v = 0; v < N; v++) {
        if (v == u) continue;
        {
            double d = dist(u, v);
            if (d <= Rmax) { c[m].id = v; c[m].d = d; m++; }
        }
    }
    qsort(c, m, sizeof(Cand), cmp_cand);
```

- Μαζεύουμε **όλους τους πιθανούς** γείτονες εντός `Rmax` (όσους θα μπορούσε να φτάσει ο
  *u* στη μέγιστη ισχύ — δηλαδή οι γείτονές του στο γράφημα **G** του paper) και τους
  ταξινομούμε κατά απόσταση.

```c
    nodes[u].nbr_count = 0;
    nodes[u].radius    = 0.0;

    for (k = 0; k < m; k++) {
        v = c[k].id;
        nodes[u].nbr[nodes[u].nbr_count]       = v;
        nodes[u].nbr_angle[nodes[u].nbr_count] = angle_to(u, v);
        nodes[u].nbr_count++;
        nodes[u].radius = c[k].d;
```

- Ξεκινάμε με μηδενική ισχύ και **προσθέτουμε γείτονες έναν-έναν** κατά αύξουσα απόσταση.
  Κάθε προσθήκη «ανεβάζει» την ακτίνα/ισχύ στο `c[k].d`. Αυτό προσομοιώνει το «ο *u*
  εκπέμπει με ολοένα μεγαλύτερη ισχύ και ανακαλύπτει γείτονες» του paper.

```c
        for (i = 0; i < nodes[u].nbr_count; i++)
            angles[i] = nodes[u].nbr_angle[i];

        if (!has_alpha_gap(angles, nodes[u].nbr_count, alpha))
            break;
    }
}
```

- Αντιγράφουμε τις γωνίες σε προσωρινό πίνακα (γιατί η `has_alpha_gap` τις **ταξινομεί** και
  δεν θέλουμε να χαλάσει η σειρά στον `nbr_angle`).
- **Συνθήκη τερματισμού (Φάση 1):** μόλις **δεν υπάρχει πια α-gap**, σταματάμε — ο *u* βρήκε
  την ελάχιστη ισχύ που καλύπτει όλους τους κώνους. Αν ο βρόχος τελειώσει χωρίς `break`, ο
  *u* είναι **κόμβος συνόρου** που έφτασε στη `Rmax` με κενό ακόμη ανοιχτό (π.χ. γωνιακοί
  κόμβοι πλέγματος, κόμβοι δακτυλίου).

> Σύνδεση με paper: η `run_cbtc` υλοποιεί ακριβώς τον βασικό κανόνα CBTC(α). Από το θεώρημα,
> για **α ≤ 150°** το γράφημα που προκύπτει διατηρεί τη συνεκτικότητα του **G**.

### Γ.8 Βελτιστοποίηση — `shrink_back` (Φάση 2 του paper)

```c
static void shrink_back(int u, double alpha) {
    double angles[MAXN];
    int i;

    while (nodes[u].nbr_count > 1) {
        int n = nodes[u].nbr_count;

        for (i = 0; i < n - 1; i++) angles[i] = nodes[u].nbr_angle[i];
        int gap_without = has_alpha_gap(angles, n - 1, alpha);

        for (i = 0; i < n; i++) angles[i] = nodes[u].nbr_angle[i];
        int gap_with = has_alpha_gap(angles, n, alpha);
```

- Υπολογίζουμε αν υπάρχει α-gap **χωρίς** τον πιο μακρινό γείτονα (`gap_without`) και **με**
  αυτόν (`gap_with`). Θυμίζουμε: οι γείτονες προστέθηκαν κατά αύξουσα απόσταση, άρα ο
  **τελευταίος** στη λίστα είναι ο πιο μακρινός (ο πιο «ακριβός» ενεργειακά).

```c
        if (gap_without && !gap_with)
            break;

        nodes[u].nbr_count--;
        if (nodes[u].nbr_count > 0)
            nodes[u].radius = dist(u, nodes[u].nbr[nodes[u].nbr_count - 1]);
        else
            nodes[u].radius = 0.0;
    }
}
```

- **Κανόνας shrink-back (paper):** ένας κόμβος συνόρου που έφτασε στη `Rmax` με κενό μπορεί
  να **κατεβάσει** ισχύ, αρκεί η αφαίρεση να **μην ανοίγει νέο α-gap** που δεν υπήρχε ήδη.
  Εδώ: αν η αφαίρεση του μακρινού **δημιουργεί** κενό ενώ χωρίς αυτόν όλα ήταν καλά
  (`gap_without && !gap_with`), τον **κρατάμε** και σταματάμε. Αλλιώς τον πετάμε και
  μειώνουμε την ακτίνα στον επόμενο πιο μακρινό.
- Αποτέλεσμα: ίδια κάλυψη/συνεκτικότητα, **λιγότερη ενέργεια** στους κόμβους συνόρου — γι'
  αυτό το `ring.txt` δείχνει τη μεγαλύτερη μείωση ενέργειας με `--shrink` (όλοι οι κόμβοι
  του είναι σε κυρτή θέση).

> Σημείωση: το paper ορίζει και άλλες δύο βελτιστοποιήσεις Φάσης 2 — **asymmetric edge
> removal** και **pairwise edge removal**. Εδώ υλοποιούμε τη shrink-back· τη συμμετρικότητα
> τη χειριζόμαστε στο `build_graph` (βλ. παρακάτω).

### Γ.9 Κατασκευή γραφήματος — `build_graph`

```c
static void build_graph(void) {
    int u, k;
    for (u = 0; u < N; u++)
        memset(adj[u], 0, sizeof(int) * N);

    for (u = 0; u < N; u++)
        for (k = 0; k < nodes[u].nbr_count; k++) {
            int v = nodes[u].nbr[k];
            adj[u][v] = adj[v][u] = 1;
        }
}
```

- Μηδενίζουμε τον πίνακα γειτνίασης και μετά για κάθε επιλεγμένη ακμή θέτουμε **και τις δύο
  κατευθύνσεις** (`adj[u][v]` και `adj[v][u]`).
- Αυτό κάνει το γράφημα **συμμετρικό/μη-κατευθυνόμενο** με λογική **OR**: κρατάμε την ακμή αν
  **έστω ένας** από τους δύο την επέλεξε. Συνδέεται με τη συζήτηση του paper για τις
  **ασύμμετρες ζεύξεις** — πολλά πρωτόκολλα απαιτούν αμφίδρομες ζεύξεις (Paper, Φάση 2,
  asymmetric edge removal· Βιβλίο §10.2.3).

### Γ.10 Έλεγχος συνεκτικότητας — `components` (BFS/DFS)

```c
static int components(void) {
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
```

- Μετράει τις **συνεκτικές συνιστώσες** με αναζήτηση (στοίβα = DFS). Για κάθε μη
  επισκεμμένο κόμβο ξεκινά νέα συνιστώσα και «απλώνεται» σε όλους τους προσβάσιμους.
- `comp == 1` σημαίνει **συνεκτικό**. Αυτή η συνάρτηση είναι το **τεστ ορθότητας** του
  θεωρήματος 5π/6: με α = 150° πρέπει να βγαίνει 1 (όταν το πλήρες γράφημα είναι συνεκτικό),
  ενώ με α > 150° μπορεί να βγει > 1 (Paper, κύριο θεώρημα).

### Γ.11 Ανάγνωση δικτύου — `load_network`

```c
static int load_network(const char *path) {
    FILE *f = fopen(path, "r");
    char line[256];
    int  got_header = 0, i = 0;

    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return 0; }
```

- Ανοίγει το αρχείο· σε αποτυχία τυπώνει σφάλμα και επιστρέφει 0.

```c
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;
```

- Διαβάζει γραμμή-γραμμή, παρακάμπτει κενά/στηλοθέτες στην αρχή και **αγνοεί σχόλια** (`#`)
  και κενές γραμμές.

```c
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
```

- Η **πρώτη** ουσιαστική γραμμή είναι η επικεφαλίδα `N Rmax`· οι υπόλοιπες είναι ζεύγη
  `x y`. Ελέγχει ότι διαβάστηκαν ακριβώς `N` κόμβοι.

### Γ.12 Η `main` — ορίσματα, ροή, μετρικές

```c
int main(int argc, char **argv) {
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
```

- Απλό parsing ορισμάτων. Προεπιλογή **α = 150°** — δηλαδή ακριβώς το κατώφλι 5π/6 του
  θεωρήματος.

```c
    if (!netfile) { /* μήνυμα χρήσης */ return 1; }
    if (!load_network(netfile)) return 1;

    alpha = alpha_deg * M_PI / 180.0;
```

- Μετατροπή μοιρών → ακτίνια, γιατί η `has_alpha_gap` δουλεύει σε ακτίνια.

```c
    for (u = 0; u < N; u++) run_cbtc(u, alpha);
    if (use_shrink)
        for (u = 0; u < N; u++) shrink_back(u, alpha);

    build_graph();
```

- **Η ροή του αλγορίθμου:** τρέξε CBTC σε **κάθε** κόμβο (το «κατανεμημένο» κομμάτι,
  προσομοιωμένο σειριακά) → προαιρετικά shrink-back → φτιάξε το γράφημα.

```c
    {
        long   deg_sum = 0;
        double rmax_used = 0.0, energy = 0.0;
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
        /* ... printf των μετρικών ... */
    }
```

- Υπολογίζει τις μετρικές αξιολόγησης:
  - **μέσος βαθμός** = `deg_sum / N` → η βασική μετρική «πόσο αραίωσε» το δίκτυο. Το βιβλίο
    (§10.2.2) εξηγεί ότι **δεν υπάρχει σταθερός «μαγικός αριθμός»**, γι' αυτό τον μετράμε.
  - **μέγιστη ακτίνα** και **συνολική ενέργεια = Σ ακτίνα²** (μοντέλο ισχύος με εκθέτη β=2)
    → δείχνουν την εξοικονόμηση, ειδικά με shrink-back.
  - **συνιστώσες** → ο έλεγχος συνεκτικότητας του θεωρήματος.

```c
    if (edgefile) {
        FILE *fe = fopen(edgefile, "w");
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
        }
    }
    return 0;
}
```

- Αν ζητήθηκε `--edges`, γράφει κάθε ακμή `u<v` με τις συντεταγμένες των δύο άκρων — ακριβώς
  ό,τι χρειάζεται ο visualiser (ή το gnuplot) για να τραβήξει γραμμές.

---

## Μέρος Δ — Επεξήγηση του `plot_topology.py`

```python
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
```

- `matplotlib.use("Agg")` → backend που **αποθηκεύει σε αρχείο** χωρίς παράθυρο/οθόνη
  (δουλεύει παντού, ακόμη και χωρίς γραφικό περιβάλλον).

```python
def load_network(path):
    rmax, pts, header = None, [], False
    with open(path) as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            if not header:
                _, rmax = s.split()[:2]; rmax = float(rmax); header = True
            else:
                x, y = s.split()[:2]; pts.append((float(x), float(y)))
    return rmax, pts
```

- Ίδια λογική με τη C `load_network`: αγνοεί σχόλια, διαβάζει `Rmax` από την επικεφαλίδα και
  μετά τα ζεύγη `x y`. Επιστρέφει τη `Rmax` και τη λίστα σημείων.

```python
def full_power_edges(pts, rmax):
    e = []
    for i in range(len(pts)):
        for j in range(i + 1, len(pts)):
            dx = pts[i][0] - pts[j][0]; dy = pts[i][1] - pts[j][1]
            if math.hypot(dx, dy) <= rmax:
                e.append((i, j))
    return e
```

- Υπολογίζει το **γράφημα πλήρους ισχύος** (όλα τα ζεύγη εντός `Rmax`) — το «πριν». Αυτό
  είναι το γράφημα **G** του paper, χωρίς κανέναν έλεγχο τοπολογίας.

```python
def load_edges(path):
    e = []
    with open(path) as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith("#"): continue
            a, b = s.split()[:2]; e.append((int(a), int(b)))
    return e
```

- Διαβάζει τις ακμές του CBTC από το αρχείο `--edges` — το «μετά».

```python
def draw(ax, pts, edges, title):
    for u, v in edges:
        ax.plot([pts[u][0], pts[v][0]], [pts[u][1], pts[v][1]],
                "-", color="#2E75B6", linewidth=0.8, zorder=1)
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    ax.scatter(xs, ys, s=30, color="#C00000", zorder=2)
    avg_deg = 2 * len(edges) / len(pts) if pts else 0
    ax.set_title(f"{title}\n{len(edges)} edges,  avg degree {avg_deg:.2f}")
    ax.set_aspect("equal")
    ax.grid(True, linestyle=":", alpha=0.4)
```

- Ζωγραφίζει πρώτα τις **ακμές** (μπλε γραμμές) και μετά τους **κόμβους** (κόκκινες κουκίδες,
  `zorder=2` ώστε να είναι από πάνω). Ο τίτλος δείχνει αριθμό ακμών και **μέσο βαθμό**
  (= 2·ακμές/κόμβους). `set_aspect("equal")` → να μην παραμορφώνονται οι γωνίες.

```python
def main():
    ...
    rmax, pts = load_network(net)
    before = full_power_edges(pts, rmax)
    after = load_edges(edgefile)
    fig, (axL, axR) = plt.subplots(1, 2, figsize=(11, 5.4))
    draw(axL, pts, before, f"Full power (Rmax={rmax:g})")
    draw(axR, pts, after, "After CBTC")
    fig.suptitle(net); fig.tight_layout(); fig.savefig(out, dpi=130)
```

- Δύο πάνελ δίπλα-δίπλα: αριστερά «πλήρης ισχύς», δεξιά «μετά CBTC». Έτσι **βλέπεις άμεσα τη
  μείωση** ακμών/βαθμού που πετυχαίνει ο αλγόριθμος — η οπτική απόδειξη της θεωρίας.

---

## Μέρος Ε — Προτεινόμενα πειράματα (σύνδεση με paper & βιβλίο)

1. **Μείωση βαθμού/ενέργειας (Paper §VI, προσομοιώσεις).** Τρέξε `random100.txt` με και
   χωρίς `--shrink` και σύγκρινε μέσο βαθμό, μέγιστη ακτίνα, ενέργεια με το γράφημα πλήρους
   ισχύος.
2. **Το κατώφλι 5π/6 (Paper, κύριο θεώρημα).** Σάρωσε `--alpha` από 120° ως 200° και
   κατάγραψε τις συνιστώσες — η συνεκτικότητα κρατά ως τις 150° και μπορεί να σπάσει πέρα από
   αυτές.
3. **Κέρδος shrink-back (Paper §IV).** Σύγκρινε ενέργεια στο `ring.txt` με/χωρίς `--shrink`
   (οι κόμβοι συνόρου ωφελούνται περισσότερο).
4. **«Μαγικοί αριθμοί» (Βιβλίο §10.2.2).** Δείξε ότι ο μέσος βαθμός σταθεροποιείται γύρω σε
   μια μικρή τιμή ανεξάρτητα από το πλήθος κόμβων.

---

## Σύνοψη αντιστοίχισης κώδικα ↔ θεωρίας

| Κώδικας | Έννοια paper / βιβλίο |
|---------|------------------------|
| `angle_to` (κανονικοποίηση [0,2π)) | πληροφορία κατεύθυνσης / AoA (Paper §I–II) |
| `has_alpha_gap` | συνθήκη κώνου γωνίας α (Paper Φάση 1· Βιβλίο §10.2.3) |
| `run_cbtc` | βασικός αλγόριθμος CBTC(α), σταδιακή αύξηση ισχύος |
| σταθερά 150° | κατώφλι 5π/6, ακριβές όριο συνεκτικότητας (Paper, θεώρημα) |
| `shrink_back` | βελτιστοποίηση Φάσης 2 (Paper §IV) |
| `build_graph` (συμμετρικό) | αμφίδρομες ζεύξεις / asymmetric edge removal |
| `components` (BFS) | έλεγχος διατήρησης συνεκτικότητας |
| μέσος βαθμός | «μαγικοί αριθμοί» / κρίσιμες παράμετροι (Βιβλίο §10.2.2) |
| ενέργεια = Σ ακτίνα² | μοντέλο ισχύος (Βιβλίο §10.1, flat power control) |
