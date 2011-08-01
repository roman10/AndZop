package feipeng.andzop.Main;

import android.content.Context;
import android.content.SharedPreferences;


public class ROISettingsStatic {
	private static final String PERFERENCE_FILE_NAME = "feipeng.andzop.main.roisettings";
	private static final String ROI_WIDTH="ROI_WIDTH";
	private static final String ROI_HEIGHT="ROI_HEIGHT";
	//get and set roi width
	public static int getROIWidth(Context context) {
		SharedPreferences prefs = context.getSharedPreferences(PERFERENCE_FILE_NAME, Context.MODE_PRIVATE);
		return prefs.getInt(ROI_WIDTH, 30);
	}
	public static void setROIWidth(Context context, int _width) {
		SharedPreferences.Editor prefs = context.getSharedPreferences(PERFERENCE_FILE_NAME , Context.MODE_PRIVATE).edit();
		prefs.putInt(ROI_WIDTH, _width);
		prefs.commit();
	}
	//get and set roi height
	public static int getROIHeight(Context context) {
		SharedPreferences prefs = context.getSharedPreferences(PERFERENCE_FILE_NAME, Context.MODE_PRIVATE);
		return prefs.getInt(ROI_HEIGHT, 30);
	}
	public static void setROIHeight(Context context, int _height) {
		SharedPreferences.Editor prefs = context.getSharedPreferences(PERFERENCE_FILE_NAME , Context.MODE_PRIVATE).edit();
		prefs.putInt(ROI_HEIGHT, _height);
		prefs.commit();
	}
	
	private static final String VIEW_MODE = "VIEW_MODE";
	public static int getViewMode(Context _context) {
		SharedPreferences prefs = _context.getSharedPreferences(PERFERENCE_FILE_NAME, Context.MODE_PRIVATE);
		return prefs.getInt(VIEW_MODE, 0);
	}
	public static void setViewMode(Context _context, int _mode) {
		SharedPreferences.Editor prefs = _context.getSharedPreferences(PERFERENCE_FILE_NAME , Context.MODE_PRIVATE).edit();
		prefs.putInt(VIEW_MODE, _mode);
		prefs.commit();
	}
}
