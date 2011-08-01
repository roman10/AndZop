package feipeng.iconifiedtextselectedlist;

import android.content.Context;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.view.Gravity;
import android.view.View;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

public class IconifiedTextSelectedView extends LinearLayout {
	
	private TextView mText;
	private ImageView mIcon;
	public CheckBox mCheckbox;
	public static int mCheckbox_id = 1000;
	public IconifiedTextSelectedView(Context context, IconifiedTextSelected iconifiedTextSelected) {
		super(context);

		/* First Icon and the Text to the right (horizontal),
		 * not above and below (vertical) */
		this.setOrientation(HORIZONTAL);

		mIcon = new ImageView(context);
		mIcon.setPadding(5, 10, 20, 10); // 5px to the right
		mIcon.setScaleType(ImageView.ScaleType.FIT_CENTER);
        mIcon.setLayoutParams(new LayoutParams(90, 90)); 
		//mIcon.setLayoutParams(new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
        mIcon.setMaxHeight(72);
        mIcon.setMaxWidth(72);
		mIcon.setImageDrawable(iconifiedTextSelected.getIcon());
//		// left, top, right, bottom

		addView(mIcon);
		mText = new TextView(context);
		mText.setTypeface(Typeface.SERIF, 0);
		mText.setText(iconifiedTextSelected.getText());
		/* Now the text (after the icon) */
		LinearLayout.LayoutParams params =  new LinearLayout.LayoutParams(
				LayoutParams.FILL_PARENT, LayoutParams.WRAP_CONTENT);
		params.weight = 1;
		params.gravity = Gravity.CENTER_VERTICAL;
		addView(mText, params);
//		
		//add the checkbox
		mCheckbox = new CheckBox(context);
		mCheckbox.setChecked(false);
		mCheckbox.setFocusable(false);
		if (iconifiedTextSelected.getVisibility()) {
			mCheckbox.setVisibility(View.VISIBLE);
		} else {
			mCheckbox.setVisibility(View.GONE);
		}
		mCheckbox.setId(mCheckbox_id);
		addView(mCheckbox, new LinearLayout.LayoutParams(
				LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
	}

	public void setText(String words) {
		mText.setText(words);
	}
	
	public void setIcon(Drawable bullet) {
		mIcon.setImageDrawable(bullet);
	}

	public void setIcon(int res_id) {
		mIcon.setImageResource(res_id);
	}
	
	public void setSelected(boolean selected) {
		mCheckbox.setChecked(selected);
	}
	
	public void setVisibility(boolean visibility) {
		if (visibility) {
			mCheckbox.setVisibility(View.VISIBLE);
		} else 
		{
			mCheckbox.setVisibility(View.GONE);
		}
	}
}