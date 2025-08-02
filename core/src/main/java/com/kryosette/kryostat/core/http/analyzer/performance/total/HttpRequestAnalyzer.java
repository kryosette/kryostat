package com.kryosette.kryostat.core.http.analyzer.performance.total;

import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;
import java.util.ArrayList;
import java.util.DoubleSummaryStatistics;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.stream.Collectors;

public class HttpRequestAnalyzer {
    private static final HttpClient httpClient = HttpClient.newBuilder()
            .connectTimeout(Duration.ofSeconds(3))
            .build();

    public static void main(String[] args) throws Exception {
        String targetUrl = "http://localhost:8092/api/rooms/11/messages";
        int requestCount = 100;

        // 1. Выполняем асинхронные запросы и собираем метрики
        List<CompletableFuture<Long>> futures = new ArrayList<>();
        for (int i = 0; i < requestCount; i++) {
            futures.add(measureRequestTime(targetUrl));
        }

        // 2. Ждём завершения всех запросов
        List<Long> responseTimes = futures.stream()
                .map(CompletableFuture::join)
                .collect(Collectors.toList());

        // 3. Анализируем результаты
        analyzeResponseTimes(responseTimes);
    }

    private static CompletableFuture<Long> measureRequestTime(String url) {
        long startTime = System.nanoTime();
        String token = "19024586-ba1e-44fe-9f09-e337f2d7793f";
        HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create(url))
                .header("Authorization", "Bearer " + token)
                .header("Content-Typing", "application/json")
                .timeout(Duration.ofSeconds(5))
                .build();

        return httpClient.sendAsync(request, HttpResponse.BodyHandlers.ofString())
                .thenApply(response -> {
                    long duration = (System.nanoTime() - startTime) / 1_000_000; // мс
                    System.out.printf("Запрос завершён за %d мс с кодом %d%n",
                            duration, response.statusCode());
                    return duration;
                });
    }

    private static void analyzeResponseTimes(List<Long> times) {
        DoubleSummaryStatistics stats = times.stream()
                .mapToDouble(Long::doubleValue)
                .summaryStatistics();

        double mean = stats.getAverage();
        double variance = times.stream()
                .mapToDouble(time -> Math.pow(time - mean, 2))
                .average()
                .orElse(0.0);
        double stdDev = Math.sqrt(variance);

        System.out.println("\n📊 Результаты анализа:");
        System.out.printf("Количество запросов: %d%n", times.size());
        System.out.printf("Среднее время: %.2f мс%n", mean);
        System.out.printf("Мин/Макс: %.2f/%.2f мс%n",
                stats.getMin(),
                stats.getMax());
        System.out.printf("Дисперсия: %.2f мс²%n", variance);
        System.out.printf("Стандартное отклонение: %.2f мс%n", stdDev);

        // Дополнительно: процент запросов выше среднего + 2σ
        double threshold = mean + 2 * stdDev;
        long slowRequests = times.stream().filter(t -> t > threshold).count();
        System.out.printf("Запросов > среднее + 2σ: %d (%.1f%%)%n",
                slowRequests, (slowRequests * 100.0 / times.size()));
    }
}