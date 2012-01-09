package feipeng.andzop.utils;

import java.io.File;

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
}
