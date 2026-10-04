package com.phonecastvr.sender;

import java.util.Collections;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

final class NotificationPolicy {
    private NotificationPolicy() {}

    static Set<String> packageSet(String value) {
        if (value == null || value.trim().isEmpty()) return Collections.emptySet();
        Set<String> packages = new HashSet<>();
        for (String item : value.split("[,\\s]+")) {
            String normalized = item.trim().toLowerCase(Locale.ROOT);
            if (!normalized.isEmpty()) packages.add(normalized);
        }
        return packages;
    }

    static boolean allows(String packageName, String allowList, String blockList) {
        if (packageName == null || packageName.isEmpty()) return false;
        String normalized = packageName.toLowerCase(Locale.ROOT);
        Set<String> blocked = packageSet(blockList);
        if (blocked.contains(normalized)) return false;
        Set<String> allowed = packageSet(allowList);
        return allowed.isEmpty() || allowed.contains(normalized);
    }

    static String clean(CharSequence value, int maximumCharacters) {
        if (value == null) return "";
        String cleaned = value.toString().replace('\n', ' ').replace('\r', ' ')
                .replaceAll("\\s+", " ").trim();
        if (cleaned.length() <= maximumCharacters) return cleaned;
        return cleaned.substring(0, maximumCharacters - 1) + "…";
    }
}
