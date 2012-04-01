package feipeng.andzop.utils;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

import android.util.Log;

public class FileUtilsStatic {
	public static void deleteAllDepFiles(String dir) {
		try {
			File[] tfiles = new File(dir).listFiles();
			for (File aFile : tfiles) {
				if (aFile.getName().endsWith(".txt")) {
					aFile.delete();
				} else if (aFile.getName().endsWith(".tmp")) {
					aFile.delete();
				}
			}
		} catch (Exception e) {
			//dir not exists or cannot delete the file
			e.printStackTrace();
		}
	}
	
	public static void appendContentToFile(String pContent, String pDir, String pName) {
		File file = new File(pDir, pName);
		try {
			BufferedWriter bw = new BufferedWriter(new FileWriter(file, true));
			bw.write(pContent);
			if (bw != null) {
				bw.close();
			}
		} catch (IOException e) {
			Log.i("appendContentToFile", e.toString());
		}
	}
}
