package com.kryosette.kryostat.core.http.config;

import lombok.Getter;
import lombok.RequiredArgsConstructor;
import lombok.Setter;
import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.context.annotation.Configuration;

@Getter
@Setter
@Configuration
@ConfigurationProperties(prefix = "http.analysis")
@RequiredArgsConstructor
public class HttpAnalysisConfigVulnerabilities {
    private boolean detectXss = true;
    private boolean detectSqlInject = true;
    private int maxHeaderCount = 50;
}
