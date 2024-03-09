package @ANDROID_MANIFEST_PACKAGE@;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;

import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import android.view.View;
import android.view.ViewGroup;
import android.view.LayoutInflater;

public class SDLEntryTestActivity extends Activity {

    public String MODIFY_ARGUMENTS = "@ANDROID_MANIFEST_PACKAGE@.MODIFY_ARGUMENTS";
    boolean isModifyingArguments;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.v("SDL", "SDLEntryTestActivity onCreate");
        super.onCreate(savedInstanceState);

        String intent_action = getIntent().getAction();
        Log.v("SDL", "SDLEntryTestActivity intent.action = " + intent_action);

        if (intent_action == MODIFY_ARGUMENTS) {
            isModifyingArguments = true;
            createArgumentLayout();
        } else {
            startChildActivityAndFinish();
        }
    }

    protected void createArgumentLayout() {
        LayoutInflater inflater = getLayoutInflater();
        View view = inflater.inflate(R.layout.arguments_layout, null);
        setContentView(view);

        Button button = (Button)requireViewById(R.id.arguments_start_button);
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                startChildActivityAndFinish();
            }
        });
    }

    protected String[] getArguments() {
        if (!isModifyingArguments) {
            return new String[0];
        }
        EditText editText = (EditText)findViewById(R.id.arguments_edit);
        String text = editText.getText().toString();
        String new_text = text.replace("[ \t]*[ \t\n]+[ \t]+", "\n").strip();
        Log.v("SDL", "text = " + text + "\n becomes \n" + new_text);
        return new_text.split("\n", 0);
    }

    @Override
    protected void onStart() {
        Log.v("SDL", "SDLEntryTestActivity onStart");
        super.onStart();
    }

    @Override
    protected void onResume() {
        Log.v("SDL", "SDLEntryTestActivity onResume");
        super.onResume();
    }

    @Override
    protected void onPause() {
        Log.v("SDL", "SDLEntryTestActivity onPause");
        super.onPause();
    }

    @Override
    protected void onStop() {
        Log.v("SDL", "SDLEntryTestActivity onStop");
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        Log.v("SDL", "SDLEntryTestActivity onDestroy");
        super.onDestroy();
    }

    @Override
    protected void onRestoreInstanceState(Bundle savedInstanceState) {
        Log.v("SDL", "SDLEntryTestActivity onRestoreInstanceState");
        super.onRestoreInstanceState(savedInstanceState);
        EditText editText = (EditText)findViewById(R.id.arguments_edit);
        editText.setText(savedInstanceState.getCharSequence("args", ""), TextView.BufferType.EDITABLE);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        Log.v("SDL", "SDLEntryTestActivity onSaveInstanceState");
        EditText editText = (EditText)findViewById(R.id.arguments_edit);
        outState.putCharSequence("args", editText.getText());
        super.onSaveInstanceState(outState);
    }

    private void startChildActivityAndFinish() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.setClassName("@ANDROID_MANIFEST_PACKAGE@", "@ANDROID_MANIFEST_PACKAGE@.SDLTestActivity");
        intent.putExtra("arguments", getArguments());
        startActivity(intent);
        finish();
    }
}
