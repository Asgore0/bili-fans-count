package com.example.bupfans;

import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Date;

final class BiliFansClient {
    static final String DEFAULT_NAME = "谐门东西";
    static final long DEFAULT_MID = 3546718146661176L;

    private BiliFansClient() {
    }

    static FanResult fetchFanResult(Target target) throws Exception {
        try {
            FanResult result = fetchCard(target.mid, target.name);
            if (result != null) {
                return result;
            }
        } catch (Exception ignored) {
            // The card endpoint carries the display name, but the relation API is the sturdier fallback.
        }
        int followers = fetchFollowers(target.mid);
        return new FanResult(target.name, target.mid, followers, new Date());
    }

    private static FanResult fetchCard(long mid, String fallbackName) throws Exception {
        String url = "https://api.bilibili.com/x/web-interface/card?mid=" + mid + "&photo=false";
        JSONObject root = new JSONObject(httpGet(url));
        if (root.optInt("code", -1) != 0) {
            return null;
        }
        JSONObject data = root.optJSONObject("data");
        if (data == null) {
            return null;
        }
        JSONObject card = data.optJSONObject("card");
        String name = fallbackName;
        int followers = -1;
        if (card != null) {
            name = cleanName(card.optString("name", fallbackName), fallbackName, mid);
            if (card.has("fans")) {
                followers = card.optInt("fans", -1);
            }
        }
        if (followers < 0 && data.has("follower")) {
            followers = data.optInt("follower", -1);
        }
        if (followers < 0) {
            return null;
        }
        return new FanResult(name, mid, followers, new Date());
    }

    private static int fetchFollowers(long mid) throws Exception {
        String url = "https://api.bilibili.com/x/relation/stat?vmid=" + mid;
        JSONObject root = new JSONObject(httpGet(url));
        if (root.optInt("code", -1) != 0) {
            throw new IllegalStateException(root.optString("message", "关注者接口返回异常"));
        }
        return root.getJSONObject("data").getInt("follower");
    }

    private static String httpGet(String address) throws Exception {
        HttpURLConnection connection = (HttpURLConnection) new URL(address).openConnection();
        try {
            connection.setConnectTimeout(10_000);
            connection.setReadTimeout(10_000);
            connection.setRequestMethod("GET");
            connection.setRequestProperty("User-Agent", "Mozilla/5.0 Android BUpFans/2.5");
            connection.setRequestProperty("Referer", "https://www.bilibili.com/");
            connection.setRequestProperty("Accept", "application/json");

            int code = connection.getResponseCode();
            InputStream stream = code >= 200 && code < 300
                    ? connection.getInputStream()
                    : connection.getErrorStream();
            String body = readAll(stream);

            if (code < 200 || code >= 300) {
                throw new IllegalStateException("HTTP " + code + ": " + body);
            }
            return body;
        } finally {
            connection.disconnect();
        }
    }

    private static String readAll(InputStream stream) throws Exception {
        if (stream == null) {
            return "";
        }
        StringBuilder builder = new StringBuilder();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(stream, StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) {
                builder.append(line);
            }
        }
        return builder.toString();
    }

    private static String cleanName(String value, String fallbackName, long mid) {
        if (value == null) {
            return fallbackName(mid, fallbackName);
        }
        String trimmed = value.trim();
        if (trimmed.isEmpty()) {
            return fallbackName(mid, fallbackName);
        }
        return trimmed.length() > 24 ? trimmed.substring(0, 24) : trimmed;
    }

    private static String fallbackName(long mid, String fallbackName) {
        if (fallbackName != null && !fallbackName.trim().isEmpty()) {
            return fallbackName.trim();
        }
        return "UID " + mid;
    }

    static final class Target {
        final String name;
        final long mid;

        Target(String name, long mid) {
            this.name = cleanName(name, null, mid);
            this.mid = mid;
        }
    }

    static final class FanResult {
        final String name;
        final long mid;
        final int followers;
        final Date updatedAt;

        FanResult(String name, long mid, int followers, Date updatedAt) {
            this.name = cleanName(name, null, mid);
            this.mid = mid;
            this.followers = followers;
            this.updatedAt = updatedAt;
        }
    }
}
