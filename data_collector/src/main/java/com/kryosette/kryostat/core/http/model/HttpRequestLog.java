package com.kryosette.kryostat.core.http.model;

import lombok.Data;

import java.time.Instant;
import java.util.Map;

@Data
public class HttpRequestLog {
    private String method;
    private String url;
    private Map<String, String> headers;
    private String body;
    private String sourceIp;
    private Instant timestamp;
}