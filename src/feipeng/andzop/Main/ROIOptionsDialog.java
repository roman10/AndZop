package feipeng.andzop.Main;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager.LayoutParams;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;

/**
 * @author roman10
 * 1. allow user to specify a password 
 * 2. allow user to specify permission: View and Export
 * 3. 
 */

public class ROIOptionsDialog extends Activity {
	private Button okBtn;
	private Button cancelBtn;
	private SeekBar bar_height;
	private SeekBar bar_width;
	private TextView text_height;
	private TextView text_width;
	
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		setContentView(R.layout.dialog_roi_options);	
		bar_height = (SeekBar) findViewById(R.id.dialog_roi_options_height_bar);
		bar_height.setProgress(ROISettingsStatic.getROIHeight(this.getApplicationContext()));
		bar_height.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
			@Override
			public void onStopTrackingTouch(SeekBar seekBar) {}
			@Override
			public void onStartTrackingTouch(SeekBar seekBar) {}
			@Override
			public void onProgressChanged(SeekBar seekBar, int progress,
					boolean fromUser) {
				text_height.setText("ROI Height: " + progress + "%");
			}
		});
		text_height = (TextView) findViewById(R.id.dialog_roi_options_height_text);
		text_height.setText("ROI Height: " + ROISettingsStatic.getROIHeight(this.getApplicationContext()) + "%");
		bar_width = (SeekBar) findViewById(R.id.dialog_roi_options_width_bar);
		bar_width.setProgress(ROISettingsStatic.getROIWidth(this.getApplicationContext()));
		bar_width.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
			@Override
			public void onStopTrackingTouch(SeekBar seekBar) {}
			@Override
			public void onStartTrackingTouch(SeekBar seekBar) {}
			@Override
			public void onProgressChanged(SeekBar seekBar, int progress,
					boolean fromUser) {
				text_width.setText("ROI Width: " + progress + "%");
			}
		});
		
		text_width = (TextView) findViewById(R.id.dialog_roi_options_width_text);
		text_width.setText("ROI Width: " + ROISettingsStatic.getROIWidth(this.getApplicationContext()) + "%");
		
		okBtn = (Button) findViewById(R.id.dialog_roi_options_btn1);
		okBtn.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				Intent result_intent = new Intent();
				setResult(RESULT_OK, result_intent);
				ROISettingsStatic.setROIHeight(getApplicationContext(), bar_height.getProgress());
				ROISettingsStatic.setROIWidth(getApplicationContext(), bar_width.getProgress());
				finish();
			}
		});
		cancelBtn = (Button) findViewById(R.id.dialog_roi_options_btn2);
		cancelBtn.setOnClickListener(new View.OnClickListener() {	
			public void onClick(View v) {
				//cancel export, just finish the activity
				ROIOptionsDialog.this.finish();
			}
		});
		getWindow().setSoftInputMode(LayoutParams.SOFT_INPUT_STATE_HIDDEN);
	}
}

