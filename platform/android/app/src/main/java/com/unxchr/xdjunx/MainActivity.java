package com.unxchr.xdjunx;

import android.app.NativeActivity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.content.pm.PackageManager;
import android.util.Log;
import android.Manifest;
import android.view.WindowManager;

public class MainActivity extends NativeActivity {
    private static final String TAG = "UNX_MainActivity";
    private static final int PERMISSION_REQUEST_CODE = 123;
    private static final int MANAGE_STORAGE_REQUEST_CODE = 124;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        Log.i(TAG, "onCreate: Requesting permissions for Android " + Build.VERSION.SDK_INT);
        requestRuntimePermissions();
        checkManageStoragePermission();
    }

    private void checkManageStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            // Android 11+ (API 30+)
            if (!Environment.isExternalStorageManager()) {
                Log.i(TAG, "requestManageStoragePermission: Requesting All Files Access");
                try {
                    Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                    intent.addCategory("android.intent.category.DEFAULT");
                    intent.setData(Uri.parse(String.format("package:%s", getPackageName())));
                    startActivityForResult(intent, MANAGE_STORAGE_REQUEST_CODE);
                } catch (Exception e) {
                    Intent intent = new Intent();
                    intent.setAction(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                    startActivityForResult(intent, MANAGE_STORAGE_REQUEST_CODE);
                }
            }
        }
    }

    private void requestRuntimePermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            // Android 13+ (API 33+)
            String[] permissions = {
                Manifest.permission.READ_MEDIA_AUDIO,
                Manifest.permission.READ_MEDIA_IMAGES,
                Manifest.permission.READ_MEDIA_VIDEO
            };
            if (!hasPermissions(permissions)) {
                requestPermissions(permissions, PERMISSION_REQUEST_CODE);
            }
        } else {
            // Android 12 and below
            String[] permissions = {
                Manifest.permission.READ_EXTERNAL_STORAGE,
                Manifest.permission.WRITE_EXTERNAL_STORAGE
            };
            if (!hasPermissions(permissions)) {
                requestPermissions(permissions, PERMISSION_REQUEST_CODE);
            }
        }
    }

    private boolean hasPermissions(String[] permissions) {
        for (String permission : permissions) {
            if (checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
                return false;
            }
        }
        return true;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERMISSION_REQUEST_CODE) {
            for (int i = 0; i < permissions.length; i++) {
                if (grantResults[i] == PackageManager.PERMISSION_GRANTED) {
                    Log.i(TAG, "Permission granted: " + permissions[i]);
                } else {
                    Log.w(TAG, "Permission denied: " + permissions[i]);
                }
            }
        }
    }
}
