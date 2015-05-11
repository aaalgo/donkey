namespace cpp donkey

struct SearchRequest {
    1:required i32 db;
    2:required string url;
    3:required string content;
    4:required i32 K;
    5:required double R;
    6:required list<double> params;
}

struct Hit {
    1:required string key;
    2:required double score;
}

struct SearchResponse {
    1:required list<Hit> hits;
}

struct InsertRequest {
    1:required i32 db;
    2:required string key;
    3:required string url;
    4:required string content;
}

struct InsertResponse {
}

struct StatusRequest {
}

struct StatusResponse {
    1:required list<double> stats;
}

service donkey {
    SearchResponse search (1:required SearchRequest request);
    InsertResponse insert (1:required InsertRequest request);
    StatusResponse status (1:required StatusRequest request);
}

