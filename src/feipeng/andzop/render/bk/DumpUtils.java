package feipeng.andzop.render.bk;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

import android.graphics.Rect;

/**
 * This class contains the utilities methods to dump log information
 * @author roman10
 *
 */
public class DumpUtils {
	public static void createDirIfNotExist(String _path) {
		File lf = new File(_path);
		try {
			if (lf.exists()) {
				//directory already exists
			} else {
				if (lf.mkdirs()) {
					//Log.v(TAG, "createDirIfNotExist created " + _path);
				} else {
					//Log.v(TAG, "createDirIfNotExist failed to create " + _path);
				}
			}
		} catch (Exception e) {
			//create directory failed
			//Log.v(TAG, "createDirIfNotExist failed to create " + _path);
		}
	}
	public static void dumpZoom(float _zoom, FileWriter _out) {
		String lStr = "" + _zoom + ":";
		try {
			_out.write(lStr);
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
	public static void dumpPanXY(float _panX, float _panY, FileWriter _out) {
		String lStr = "" + _panX + "," + _panY + ":";
		try {
			_out.write(lStr);
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
	public static void dumpROI(Rect _roi, FileWriter _out) {
		String lStr = "" + _roi.top + "," + _roi.left + "," + _roi.bottom + "," + _roi.right + ":";
		try {
			_out.write(lStr + "\n");
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

}
