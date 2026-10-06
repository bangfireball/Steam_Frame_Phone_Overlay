package com.phonecastvr.sender;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.Context;
import android.content.Intent;
import android.widget.RemoteViews;

public final class QuickConnectWidget extends AppWidgetProvider {
    @Override public void onUpdate(Context context, AppWidgetManager manager, int[] appWidgetIds) {
        for (int appWidgetId : appWidgetIds) {
            Intent connect = new Intent(context, MainActivity.class)
                    .setAction(MainActivity.ACTION_QUICK_CONNECT)
                    .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            PendingIntent pendingConnect = PendingIntent.getActivity(context, appWidgetId, connect,
                    PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
            RemoteViews views = new RemoteViews(context.getPackageName(),
                    R.layout.quick_connect_widget);
            views.setOnClickPendingIntent(R.id.quick_connect_button, pendingConnect);
            manager.updateAppWidget(appWidgetId, views);
        }
    }
}
