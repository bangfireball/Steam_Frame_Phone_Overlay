package com.phonecastvr.sender;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.RemoteViews;

public final class QuickConnectWidget extends AppWidgetProvider {
    private static final int EXPANDED_LABEL_WIDTH_DP = 96;

    @Override public void onUpdate(Context context, AppWidgetManager manager, int[] appWidgetIds) {
        for (int appWidgetId : appWidgetIds) update(context, manager, appWidgetId);
    }

    @Override public void onAppWidgetOptionsChanged(Context context, AppWidgetManager manager,
                                                     int appWidgetId, Bundle newOptions) {
        update(context, manager, appWidgetId);
    }

    private static void update(Context context, AppWidgetManager manager, int appWidgetId) {
        Intent connect = new Intent(context, MainActivity.class)
                .setAction(MainActivity.ACTION_QUICK_CONNECT)
                .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent pendingConnect = PendingIntent.getActivity(context, appWidgetId, connect,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        RemoteViews views = new RemoteViews(context.getPackageName(),
                R.layout.quick_connect_widget);
        int width = manager.getAppWidgetOptions(appWidgetId)
                .getInt(AppWidgetManager.OPTION_APPWIDGET_MIN_WIDTH, 40);
        views.setViewVisibility(R.id.quick_connect_label,
                width >= EXPANDED_LABEL_WIDTH_DP ? View.VISIBLE : View.GONE);
        views.setOnClickPendingIntent(R.id.quick_connect_root, pendingConnect);
        manager.updateAppWidget(appWidgetId, views);
    }
}
