#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

/* ---------- limits / sizes ---------- */

#define MAX_WAREHOUSE_ROWS 512
#define MAX_RELIEF_ROWS    512
#define MAX_EDGES          1024
#define MAX_CITIES         256
#define MAX_RESOURCES      64
#define MAX_NAME_LEN       64
#define MAX_LINE_LEN       512
#define INF_DIST           1e18

/* ---------- data structures ---------- */

typedef struct {
    char warehouse[MAX_NAME_LEN];
    char city[MAX_NAME_LEN];
    char resource[MAX_NAME_LEN];
    int  qty;           /* remaining stock */
} WarehouseRow;

typedef struct {
    char area[MAX_NAME_LEN];
    char city[MAX_NAME_LEN];
    char resource[MAX_NAME_LEN];
    int  requested;
    int  remaining;
    int  people;
    int  urgency;       /* 0–100 */
} ReliefRow;

typedef struct {
    char from[MAX_NAME_LEN];
    char to[MAX_NAME_LEN];
    double dist;
} Edge;

typedef struct {
    char name[MAX_NAME_LEN];
} City;

typedef struct {
    char name[MAX_NAME_LEN];
    long long requested;
    long long allocated;
} ResourceAgg;

/* ---------- globals for simplicity ---------- */

WarehouseRow g_wh[MAX_WAREHOUSE_ROWS];
int g_wh_count = 0;

ReliefRow g_rf[MAX_RELIEF_ROWS];
int g_rf_count = 0;

Edge g_edges[MAX_EDGES];
int g_edge_count = 0;

City g_cities[MAX_CITIES];
int g_city_count = 0;

ResourceAgg g_res[MAX_RESOURCES];
int g_res_count = 0;

/* ---------- tiny helpers ---------- */

static void trim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1]=='\n'||s[n-1]=='\r'||s[n-1]==' '||s[n-1]=='\t')) {
        s[--n] = '\0';
    }
    size_t i = 0;
    while (s[i]==' '||s[i]=='\t') i++;
    if (i) memmove(s, s+i, strlen(s+i)+1);
}

/* city index, auto-create */
static int city_index(const char *name) {
    for (int i = 0; i < g_city_count; ++i) {
        if (strcmp(g_cities[i].name, name) == 0) return i;
    }
    if (g_city_count >= MAX_CITIES) return -1;
    strncpy(g_cities[g_city_count].name, name, MAX_NAME_LEN-1);
    g_cities[g_city_count].name[MAX_NAME_LEN-1] = '\0';
    return g_city_count++;
}

/* resource aggregate index, auto-create */
static int resource_index(const char *name) {
    for (int i = 0; i < g_res_count; ++i) {
        if (strcmp(g_res[i].name, name) == 0) return i;
    }
    if (g_res_count >= MAX_RESOURCES) return -1;
    strncpy(g_res[g_res_count].name, name, MAX_NAME_LEN-1);
    g_res[g_res_count].name[MAX_NAME_LEN-1] = '\0';
    g_res[g_res_count].requested = 0;
    g_res[g_res_count].allocated = 0;
    return g_res_count++;
}

/* ---------- CSV parsing ---------- */

static void load_warehouses(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open warehouses file: %s\n", path);
        exit(1);
    }
    char line[MAX_LINE_LEN];
    int lineNo = 0;
    while (fgets(line, sizeof(line), f)) {
        lineNo++;
        if (lineNo == 1) continue; /* header */
        trim(line);
        if (!line[0]) continue;
        char *p = line;
        char *tok;
        WarehouseRow row = {0};

        /* WarehouseName */
        tok = strsep(&p, ",");
        if (!tok) continue;
        trim(tok);
        strncpy(row.warehouse, tok, MAX_NAME_LEN-1);

        /* City */
        tok = strsep(&p, ",");
        if (!tok) continue;
        trim(tok);
        strncpy(row.city, tok, MAX_NAME_LEN-1);

        /* Resource */
        tok = strsep(&p, ",");
        if (!tok) continue;
        trim(tok);
        strncpy(row.resource, tok, MAX_NAME_LEN-1);

        /* Quantity */
        tok = strsep(&p, ",");
        if (!tok) continue;
        trim(tok);
        row.qty = atoi(tok);
        if (row.qty < 0) row.qty = 0;

        if (!row.warehouse[0] || !row.city[0] || !row.resource[0]) continue;
        if (g_wh_count < MAX_WAREHOUSE_ROWS) {
            g_wh[g_wh_count++] = row;
            city_index(row.city);
        }
    }
    fclose(f);
}

static void load_relief(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open relief file: %s\n", path);
        exit(1);
    }
    char line[MAX_LINE_LEN];
    int lineNo = 0;
    while (fgets(line, sizeof(line), f)) {
        lineNo++;
        if (lineNo == 1) continue;
        trim(line);
        if (!line[0]) continue;

        char *p = line;
        char *tok;
        ReliefRow row = {0};

        /* AreaName */
        tok = strsep(&p, ",");
        if (!tok) continue;
        trim(tok);
        strncpy(row.area, tok, MAX_NAME_LEN-1);

        /* City */
        tok = strsep(&p, ",");
        if (!tok) continue;
        trim(tok);
        strncpy(row.city, tok, MAX_NAME_LEN-1);

        /* Resource */
        tok = strsep(&p, ",");
        if (!tok) continue;
        trim(tok);
        strncpy(row.resource, tok, MAX_NAME_LEN-1);

        /* Qty */
        tok = strsep(&p, ",");
        if (!tok) continue;
        trim(tok);
        row.requested = atoi(tok);
        if (row.requested < 0) row.requested = 0;
        row.remaining = row.requested;

        /* People */
        tok = strsep(&p, ",");
        row.people = tok ? atoi(tok) : 0;

        /* Urgency */
        tok = strsep(&p, ",");
        row.urgency = tok ? atoi(tok) : 0;
        if (row.urgency < 0) row.urgency = 0;
        if (row.urgency > 100) row.urgency = 100;

        if (!row.area[0] || !row.city[0] || !row.resource[0]) continue;
        if (g_rf_count < MAX_RELIEF_ROWS) {
            g_rf[g_rf_count++] = row;
            city_index(row.city);

            int idx = resource_index(row.resource);
            if (idx >= 0) g_res[idx].requested += row.requested;
        }
    }
    fclose(f);
}

static void load_routes(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open routes file: %s\n", path);
        exit(1);
    }
    char line[MAX_LINE_LEN];
    int lineNo = 0;
    while (fgets(line, sizeof(line), f)) {
        lineNo++;
        if (lineNo == 1) continue;
        trim(line);
        if (!line[0]) continue;
        char *p = line;
        char *tok;
        Edge e = {0};

        /* From */
        tok = strsep(&p, ",");
        if (!tok) continue;
        trim(tok);
        strncpy(e.from, tok, MAX_NAME_LEN-1);

        /* To */
        tok = strsep(&p, ",");
        if (!tok) continue;
        trim(tok);
        strncpy(e.to, tok, MAX_NAME_LEN-1);

        /* Distance */
        tok = strsep(&p, ",");
        if (!tok) continue;
        trim(tok);
        e.dist = atof(tok);
        if (e.dist < 0) continue;

        if (!e.from[0] || !e.to[0]) continue;
        if (g_edge_count < MAX_EDGES) {
            g_edges[g_edge_count++] = e;
            city_index(e.from);
            city_index(e.to);
        }
    }
    fclose(f);
}

/* ---------- graph / Dijkstra ---------- */

typedef struct {
    int to;
    double w;
} AdjItem;

typedef struct {
    AdjItem items[MAX_EDGES];
    int count;
} AdjList;

static AdjList g_adj[MAX_CITIES];

static void build_graph(void) {
    for (int i = 0; i < g_city_count; ++i) g_adj[i].count = 0;
    for (int i = 0; i < g_edge_count; ++i) {
        int a = city_index(g_edges[i].from);
        int b = city_index(g_edges[i].to);
        if (a < 0 || b < 0) continue;
        double d = g_edges[i].dist;
        AdjItem ia = {b, d};
        AdjItem ib = {a, d};
        if (g_adj[a].count < MAX_EDGES) g_adj[a].items[g_adj[a].count++] = ia;
        if (g_adj[b].count < MAX_EDGES) g_adj[b].items[g_adj[b].count++] = ib;
    }
}

/* very simple O(V^2) Dijkstra; fine for small graphs */
static double dijkstra(int src, int dst, int *prev) {
    int n = g_city_count;
    double dist[MAX_CITIES];
    int used[MAX_CITIES];

    for (int i = 0; i < n; ++i) {
        dist[i] = INF_DIST;
        used[i] = 0;
        if (prev) prev[i] = -1;
    }
    dist[src] = 0.0;

    for (int it = 0; it < n; ++it) {
        int u = -1;
        double best = INF_DIST;
        for (int i = 0; i < n; ++i) {
            if (!used[i] && dist[i] < best) {
                best = dist[i];
                u = i;
            }
        }
        if (u == -1) break;
        used[u] = 1;
        if (u == dst) break;

        AdjList *L = &g_adj[u];
        for (int j = 0; j < L->count; ++j) {
            int v = L->items[j].to;
            double w = L->items[j].w;
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                if (prev) prev[v] = u;
            }
        }
    }
    return dist[dst];
}

/* reconstruct path into array of city indices; returns length */
static int build_path(int src, int dst, int *prev, int *out) {
    int len = 0;
    int cur = dst;
    while (cur != -1) {
        out[len++] = cur;
        if (cur == src) break;
        cur = prev[cur];
    }
    if (cur == -1) return 0; /* no path */
    /* reverse */
    for (int i = 0; i < len/2; ++i) {
        int t = out[i];
        out[i] = out[len-1-i];
        out[len-1-i] = t;
    }
    return len;
}

/* ---------- allocation ---------- */

typedef struct {
    int whIndex;        /* index into g_wh */
    int rfIndex;        /* index into g_rf */
    int qty;            /* allocated qty */
    double dist;        /* km */
    int pathCities[64];
    int pathLen;
} AllocationRow;

static AllocationRow g_alloc[2048];
static int g_alloc_count = 0;

static int cmp_relief(const void *a, const void *b) {
    const ReliefRow *r1 = (const ReliefRow *)a;
    const ReliefRow *r2 = (const ReliefRow *)b;
    if (r1->urgency != r2->urgency) return r2->urgency - r1->urgency; /* high first */
    return r2->people - r1->people; /* more people first */
}

static void run_allocation(void) {
    /* sort relief by urgency / people */
    qsort(g_rf, g_rf_count, sizeof(ReliefRow), cmp_relief);

    for (int i = 0; i < g_rf_count; ++i) {
        ReliefRow *need = &g_rf[i];
        int needIdx = i;
        if (need->remaining <= 0) continue;

        int cityNeed = city_index(need->city);
        if (cityNeed < 0) continue;

        /* candidate warehouses for this resource */
        int candidates[MAX_WAREHOUSE_ROWS];
        double distances[MAX_WAREHOUSE_ROWS];
        int candCount = 0;

        for (int w = 0; w < g_wh_count; ++w) {
            if (g_wh[w].qty <= 0) continue;
            if (strcmp(g_wh[w].resource, need->resource) != 0) continue;
            int cityW = city_index(g_wh[w].city);
            if (cityW < 0) continue;
            int prev[MAX_CITIES];
            double d = dijkstra(cityW, cityNeed, prev);
            if (d >= INF_DIST) continue; /* unreachable */
            candidates[candCount] = w;
            distances[candCount] = d;
            candCount++;
        }

        if (!candCount) continue;

        /* sort candidates by distance ASC */
        for (int a = 0; a < candCount; ++a) {
            for (int b = a+1; b < candCount; ++b) {
                if (distances[b] < distances[a]) {
                    double td = distances[a];
                    distances[a] = distances[b];
                    distances[b] = td;
                    int ti = candidates[a];
                    candidates[a] = candidates[b];
                    candidates[b] = ti;
                }
            }
        }

        /* allocate greedily */
        for (int c = 0; c < candCount && need->remaining > 0; ++c) {
            int widx = candidates[c];
            WarehouseRow *wh = &g_wh[widx];
            if (wh->qty <= 0) continue;

            int take = wh->qty < need->remaining ? wh->qty : need->remaining;
            if (take <= 0) continue;

            int prev[MAX_CITIES];
            int cityW = city_index(wh->city);
            double d = dijkstra(cityW, cityNeed, prev);
            if (d >= INF_DIST) continue;

            int pathIdx[64];
            int len = build_path(cityW, cityNeed, prev, pathIdx);

            AllocationRow ar;
            memset(&ar, 0, sizeof(ar));
            ar.whIndex = widx;
            ar.rfIndex = needIdx;
            ar.qty = take;
            ar.dist = d;
            ar.pathLen = len;
            for (int k = 0; k < len && k < 64; ++k)
                ar.pathCities[k] = pathIdx[k];

            if (g_alloc_count < (int)(sizeof(g_alloc)/sizeof(g_alloc[0]))) {
                g_alloc[g_alloc_count++] = ar;
            }

            wh->qty -= take;
            need->remaining -= take;

            int ridx = resource_index(need->resource);
            if (ridx >= 0) g_res[ridx].allocated += take;
        }
    }
}

/* ---------- JSON output ---------- */

static void json_escape(const char *s) {
    /* assume simple ASCII without quotes for now */
    fputs(s, stdout);
}

static void emit_json(void) {
    long long totalReq = 0, totalAlloc = 0;
    for (int i = 0; i < g_rf_count; ++i) totalReq += g_rf[i].requested;
    for (int i = 0; i < g_res_count; ++i) totalAlloc += g_res[i].allocated;

    double pct = 0.0;
    if (totalReq > 0) pct = (double)totalAlloc * 100.0 / (double)totalReq;

    fputs("{\"allocations\":[", stdout);

    for (int i = 0; i < g_alloc_count; ++i) {
        AllocationRow *ar = &g_alloc[i];
        WarehouseRow *wh = &g_wh[ar->whIndex];
        ReliefRow *rf = &g_rf[ar->rfIndex];

        const char *status;
        if (rf->remaining == 0)
            status = "Met";
        else if (rf->remaining < rf->requested)
            status = "Partial";
        else
            status = "Unmet";

        if (i > 0) fputc(',', stdout);
        fputc('{', stdout);

        /* basic fields; assume safe strings */
        fputs("\"sourceType\":\"Warehouse\",", stdout);
        fputs("\"destType\":\"ReliefCenter\",", stdout);

        fputs("\"center\":\"", stdout);  json_escape(wh->warehouse); fputs("\",", stdout);
        fputs("\"sourceCity\":\"", stdout); json_escape(wh->city); fputs("\",", stdout);
        fputs("\"area\":\"", stdout);   json_escape(rf->area); fputs("\",", stdout);
        fputs("\"destCity\":\"", stdout); json_escape(rf->city); fputs("\",", stdout);

        fputs("\"resource\":\"", stdout); json_escape(rf->resource); fputs("\",", stdout);
        fprintf(stdout, "\"requested\":%d,", rf->requested);
        fprintf(stdout, "\"allocated\":%d,", ar->qty);
        fprintf(stdout, "\"distanceKm\":%.2f,", ar->dist);
        fputs("\"status\":\"", stdout); json_escape(status); fputs("\",", stdout);

        /* path */
        fputs("\"path\":[", stdout);
        for (int k = 0; k < ar->pathLen; ++k) {
            if (k > 0) fputc(',', stdout);
            fputc('\"', stdout);
            json_escape(g_cities[ar->pathCities[k]].name);
            fputc('\"', stdout);
        }
        fputs("]}", stdout);
    }

    fputs(" ,\"summary\":{", stdout);
    fprintf(stdout, "\"totalRequested\":%lld,", totalReq);
    fprintf(stdout, "\"totalAllocated\":%lld,", totalAlloc);
    fprintf(stdout, "\"coveragePct\":%.0f,", pct);

    /* resource maps */
    fputs("\"byResourceRequested\":{", stdout);
    for (int i = 0; i < g_res_count; ++i) {
        if (i > 0) fputc(',', stdout);
        fputc('\"', stdout);
        json_escape(g_res[i].name);
        fputc('\"', stdout);
        fprintf(stdout, ":%lld", g_res[i].requested);
    }
    fputs("},", stdout);

    fputs("\"byResourceAllocated\":{", stdout);
    for (int i = 0; i < g_res_count; ++i) {
        if (i > 0) fputc(',', stdout);
        fputc('\"', stdout);
        json_escape(g_res[i].name);
        fputc('\"', stdout);
        fprintf(stdout, ":%lld", g_res[i].allocated);
    }
    fputs("}}}", stdout);
}

/* ---------- main ---------- */

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr,
                "Usage: %s warehouses.csv relief.csv routes.csv\n", argv[0]);
        return 1;
    }

    load_warehouses(argv[1]);
    load_relief(argv[2]);
    load_routes(argv[3]);
    build_graph();
    run_allocation();
    emit_json();
    return 0;
}
