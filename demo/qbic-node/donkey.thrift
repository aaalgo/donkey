namespace cpp donkey.api

struct PingRequest {
}

struct PingResponse {
}

struct SearchRequest {
    1:required i32 db;
    2:required bool raw;
    3:required string url;
    4:required string content;
    5:required string type;
    6:optional i32 K;
    7:optional double R;
    8:optional i32 hint_K;
    9:optional double hint_R;
}

struct Hit {
    1:required string key;
    2:required string meta;
    3:required double score;
}

struct SearchResponse {
    1:required double time;
    2:required double load_time;
    3:required double filter_time;
    4:required double rank_time;
    5:required list<Hit> hits;
}

struct InsertRequest {
    1:required i32 db;
    2:required string key;
    3:required bool raw;
    4:required string url;
    5:required string content;
    6:required string meta;
    7:required string type;
}

struct InsertResponse {
    1:required double time;
    2:required double load_time;
    3:required double journal_time;
    4:required double index_time;
}

struct MiscRequest {
    1:required string method;
    2:required i32 db;
}

struct MiscResponse {
    1:required i32 code;
    2:required string text;
}

exception Exception {
      1: i32 what;
      2: string why;
}

service Donkey {
    PingResponse ping (1:required PingRequest request);
    SearchResponse search (1:required SearchRequest request) throws (1:Exception e);
    InsertResponse insert (1:required InsertRequest request) throws (1:Exception e);
    MiscResponse misc (1:required MiscRequest request) throws (1:Exception e);
}

