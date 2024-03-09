package @ANDROID_MANIFEST_PACKAGE@;

import org.libsdl.app.SDLActivity;

import android.os.Bundle;
import android.util.Log;

public class SDLTestActivity extends SDLActivity {
    private String[] m_arguments;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        m_arguments = getIntent().getStringArrayExtra("arguments");
        if (m_arguments == null) {
            m_arguments = new String[0];
        }
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { getString(R.string.lib_name) };
    }

    @Override
    protected String[] getArguments() {
        Log.v("SDLTest", "#arguments = " + m_arguments.length);
        for(int i = 0; i < m_arguments.length; i++) {
            Log.v("SDLTest", "argument[" + i + "] = " + m_arguments[i]);
        }
        return m_arguments;
    }
}
