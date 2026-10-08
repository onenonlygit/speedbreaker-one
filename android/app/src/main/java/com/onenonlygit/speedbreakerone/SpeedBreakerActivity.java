package com.onenonlygit.speedbreakerone;
import android.os.Bundle;
import android.content.Intent;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.view.WindowManager;
import org.libsdl.app.SDLActivity;
public final class SpeedBreakerActivity extends SDLActivity {
    private static final int DISC_REQUEST = 7301;
    private volatile String pendingUri;
    private volatile String pendingName = "Disc image";
    private static native void nativeDiscResult(int fd, String name, String error);

    // Called from the SDL thread. SAF grants access to this one document;
    // no storage-wide permission and no ISO copy are needed.
    public void pickDisc() {
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
                            Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            try { startActivityForResult(intent, DISC_REQUEST); }
            catch (RuntimeException e) { nativeDiscResult(-1, "", "The Android file picker could not open."); }
        });
    }

    private int openDisc(Uri uri) throws java.io.IOException {
        try (ParcelFileDescriptor document = getContentResolver().openFileDescriptor(uri, "r")) {
            if (document == null) throw new java.io.IOException("No readable file descriptor.");
            pendingUri = uri.toString();
            pendingName = "Disc image";
            try (android.database.Cursor cursor = getContentResolver().query(uri,
                    new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) pendingName = cursor.getString(0);
            } catch (RuntimeException ignored) { }
            return document.detachFd(); // native code owns and closes it
        }
    }

    public int openSavedDisc() {
        String saved = getPreferences(MODE_PRIVATE).getString("disc_uri", null);
        if (saved == null) return -1;
        try { return openDisc(Uri.parse(saved)); }
        catch (java.io.IOException | RuntimeException e) { return -1; }
    }

    public String discName() { return pendingName; }

    public void rememberDisc() {
        String uri = pendingUri;
        if (uri == null) return;
        String previous = getPreferences(MODE_PRIVATE).getString("disc_uri", null);
        getPreferences(MODE_PRIVATE).edit().putString("disc_uri", uri).apply();
        if (previous != null && !previous.equals(uri)) {
            try { getContentResolver().releasePersistableUriPermission(Uri.parse(previous), Intent.FLAG_GRANT_READ_URI_PERMISSION); }
            catch (RuntimeException ignored) { }
        }
    }

    public void abandonDisc() {
        String uri = pendingUri;
        pendingUri = null;
        if (uri == null) return;
        String saved = getPreferences(MODE_PRIVATE).getString("disc_uri", null);
        if (uri.equals(saved)) getPreferences(MODE_PRIVATE).edit().remove("disc_uri").apply();
        try { getContentResolver().releasePersistableUriPermission(Uri.parse(uri), Intent.FLAG_GRANT_READ_URI_PERMISSION); }
        catch (RuntimeException ignored) { }
    }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        if (request != DISC_REQUEST) { super.onActivityResult(request, result, data); return; }
        if (result != RESULT_OK || data == null || data.getData() == null) {
            nativeDiscResult(-1, "", "");
            return;
        }
        Uri uri = data.getData();
        try {
            getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            int fd = openDisc(uri);
            nativeDiscResult(fd, pendingName, "");
        } catch (java.io.IOException | RuntimeException e) {
            pendingUri = uri.toString();
            abandonDisc();
            nativeDiscResult(-1, "", "Cannot read the selected file. Choose a local ISO on internal storage or the SD card.");
        }
    }
    @Override protected String[] getLibraries() { return new String[] {"SDL3", "main"}; }
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        WindowManager.LayoutParams params = getWindow().getAttributes();
        params.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        getWindow().setAttributes(params);
    }
}
