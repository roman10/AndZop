package feipeng.andzop.Main;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import feipeng.andzop.utils.FileUtilsStatic;
import feipeng.iconifiedtextselectedlist.IconifiedTextSelected;
import feipeng.iconifiedtextselectedlist.IconifiedTextSelectedView;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ListActivity;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.DialogInterface.OnClickListener;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.AsyncTask.Status;
import android.provider.MediaStore;
import android.text.SpannableStringBuilder;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.AbsListView;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.AbsListView.OnScrollListener;

public class VideoBrowser2 extends ListActivity implements ListView.OnScrollListener {

	private Context mContext;
	public static VideoBrowser2 self;
	
	/**
	 * activity life cycle: this part of the source code deals with activity life cycle
	 */
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		mContext = this.getApplicationContext();
		self = this;
		initUI();
	}
	
	@Override
	protected void onDestroy() {
		super.onDestroy();
		unbindDisplayEntries();
	}
	
	public void unbindDisplayEntries() {
		if (displayEntries!=null) {
			int l_count = displayEntries.size();
			for (int i = 0; i < l_count; ++i) {
				IconifiedTextSelected l_its = displayEntries.get(i);
				if (l_its != null) {
					Drawable l_dr = l_its.getIcon();
					if (l_dr != null) {
						l_dr.setCallback(null);
						l_dr = null;
					}
				}
			}
		}
	}

	/**
	 * Data: this part of the code deals with data processing
	 */
	/**
	 * Data: this part of the code deals with data processing
	 */
	//todo: it might be better if we clean up the data at destroy
	//todo: if we save the data, could we speed up the initial loading???
	//displayEntries and mSelected stores the current display data
	public static List<IconifiedTextSelected> displayEntries = new ArrayList<IconifiedTextSelected>();
	public static List<Boolean> mSelected = new ArrayList<Boolean>();
	
	public static void setEntrySelected(int pos) {
		mSelected.set(pos, true);
	}
	
	public static boolean isEntrySelected(int pos) {
		return mSelected.get(pos);
	}
	
	public static void setEntryUnselected(int pos) {
		mSelected.set(pos, false);
	}
	
	public static void setAllEntriesSelected() {
		for (int i = 0; i < mSelected.size(); ++i) {
			mSelected.set(i, true);
		}
	}
	
	public static void setAllEntriesUnselected() {
		for (int i = 0; i < mSelected.size(); ++i) {
			mSelected.set(i, false);
		}
	}
	
	private boolean checkIfAllEntriesSelected() {
		for (int i = 0; i < mSelected.size(); ++i) {
			if  (!mSelected.get(i)) {
				return false;
			}
		}
		return true;
	}

	private int getNumOfEntriesSelected() {
		int l_count = mSelected.size();
		int l_selected = 0;
		for (int i = 0; i < l_count; ++i) {
			if (mSelected.get(i)) {
				++l_selected;
			}
		}
		return l_selected;
	}
	
	/**load images
	 * this part of code deals with loading of images
	 */
	private File currentDirectory = new File("/sdcard/multitest");
	public int media_browser_load_option = 0;
	private static int last_media_browser_load_option = 0;
	private static int number_of_icons = 0;
	private static final String upOneLevel = "..";
	
	LoadImageVideoTask loadTask;
	private void loadVideosFromDirectory(String _dir) {
		try {
			if (loadTask != null && loadTask.getStatus() != Status.FINISHED) {
				//if it's already loading secrets, ignore the request
				return;
			}
			loadTask = new LoadImageVideoTask();
			loadTask.execute(_dir);
		} catch (Exception e) {
			Toast.makeText(this, "Load media fail!", Toast.LENGTH_SHORT).show();
		}
	}
	
	private void getVideosFromDirectoryNonRecur(File _dir, boolean _includeFolder) {
		Drawable folderIcon = this.getResources().getDrawable(R.drawable.normalfolder);
		//add the 
		if (!_dir.isDirectory()) {
			return;
		}
		File[] files = _dir.listFiles();
		if (files == null) {
			return;
		}
		Drawable videoIcon = null;
		int l_iconType = 0;
		for (File currentFile : files) {
			if (currentFile.isDirectory() && _includeFolder) {
				//if it's a directory
				VideoBrowser2.displayEntries.add(new IconifiedTextSelected(
						currentFile.getPath(),
						folderIcon, false, false, 0, -1, 1));
				mSelected.add(false);
			} else {
				String l_filename = currentFile.getName();
				if (checkEndsWithInStringArray(l_filename,
							getResources().getStringArray(R.array.fileEndingVideo))) {
					if (number_of_icons < 10) {
						videoIcon = null;
						++number_of_icons;
						l_iconType = 22;
					} else {
						videoIcon = null;
						l_iconType = 2;
					}
					VideoBrowser2.displayEntries.add(new IconifiedTextSelected(
							currentFile.getPath(),
							videoIcon, false, true, l_iconType, 1, -1));
					mSelected.add(false);
				}
			}
		}
	}
	
	private void getVideosFromDirectoryRecur(File _dir) {
		Drawable videoIcon = null;
		File[] files = _dir.listFiles();
		int l_iconType = 2;
		if (files == null) {
			return;
		}
		for (File currentFile : files) {
			if (currentFile.isDirectory()) {
				getVideosFromDirectoryRecur(currentFile);
				continue;
			} else {
				String l_filename = currentFile.getName();
				//if it's an image file
				if (checkEndsWithInStringArray(l_filename,
						getResources().getStringArray(R.array.fileEndingVideo))) {
					if (number_of_icons < 10) {
						videoIcon = null;
						++number_of_icons;
						l_iconType = 22;
					} else {
						videoIcon = null;
						l_iconType = 2;
					}
					VideoBrowser2.displayEntries.add(new IconifiedTextSelected(
							currentFile.getPath(),
							videoIcon, false, true, l_iconType, 1, -1));
					mSelected.add(false);
				}
			}
		}
	}
	
	private void getVideosFromGallery() {
		Drawable videoIcon = null;
		Uri uri = MediaStore.Video.Media.EXTERNAL_CONTENT_URI;
		String[] projection = {MediaStore.Video.Media.DATA};
		Cursor l_cursor = this.managedQuery(uri, projection, null, null, null);
		int videoNameColumnIndex;
		String videoFilename;
		File videoFile;
		int l_iconType = 2;
		if (l_cursor!=null) {
			if (l_cursor.moveToFirst()) {
				do {
					videoNameColumnIndex = l_cursor.getColumnIndexOrThrow(
							MediaStore.Images.Media.DATA);
					videoFilename = l_cursor.getString(videoNameColumnIndex);
					if (!checkEndsWithInStringArray(videoFilename,
							getResources().getStringArray(R.array.fileEndingVideo))) {
						continue;
					}
					videoFile = new File(videoFilename);
					if (!videoFile.exists()) {
						continue;
					}
					if (number_of_icons <= 10) {
						videoIcon = null;
						++number_of_icons;
						l_iconType = 22;
					} else {
						videoIcon = null;
						l_iconType = 2;
					}
					VideoBrowser2.displayEntries.add(new IconifiedTextSelected(
							videoFile.getAbsolutePath(),
							videoIcon, false, true, l_iconType, 1, -1));
					mSelected.add(false);
				} while (l_cursor.moveToNext());
			}
		}
		if (l_cursor!=null) {
			l_cursor.close();
		}
	}
	
	private boolean checkEndsWithInStringArray(String _checkItsEnd, 
			String[] _fileEndings){
		String checkItsEnd = _checkItsEnd.toLowerCase();
		for(String aEnd : _fileEndings){
			if(checkItsEnd.endsWith(aEnd))
				return true;
		}
		return false;
	}
	
	private class LoadImageVideoTask extends AsyncTask<String, Void, Void> {
		@Override
		protected void onPreExecute() {
			System.gc();
			displayEntries.clear();
			mSelected.clear();
			showDialog(DIALOG_LOAD_MEDIA);
		}
		@Override
		protected Void doInBackground(String... params) {
			File l_root = new File(params[0]);
			if (l_root.isDirectory()) {
				number_of_icons = 0;
				if (currentDirectory == null) {
					currentDirectory = l_root;
				}
				if (media_browser_load_option == 0) {
					//list all pictures in the root directory without going into sub folder
					getVideosFromDirectoryNonRecur(currentDirectory, true);
					//getVideosFromDirectoryNonRecur(l_root, true);
				} else if (media_browser_load_option == 1) {
					//list all pictures in the root folder recursively
					getVideosFromDirectoryRecur(l_root);
				} else if (media_browser_load_option == 2) {
					//list all pictures in the gallery
					getVideosFromGallery();
				}
			}
			return null;
		}
		@Override
		protected void onPostExecute(Void n) {
			refreshUI();
			try {
				dismissDialog(DIALOG_LOAD_MEDIA);
			} catch (Exception e) {
				Log.v("VideoBrowser2", e.getLocalizedMessage());
			}
			//update the display message
			String lMsg = "";
			if (media_browser_load_option == 0) {
				//browse
				lMsg = currentDirectory.getAbsoluteFile().toString();
			} else  {
				//load from sdcard or gallery
				lMsg = "AndZop - " + displayEntries.size() + " videos";
			}
			//get number of selected
			int pNumOfSelected = getNumOfEntriesSelected();
			text_titlebar_text.setText(lMsg + " - " + pNumOfSelected + " sel");
		}
	}
	
	/**
	 * UI: this part of the source code deals with UI
	 */
	//bottom menu
	private int currentFocusedBtn = 1;
	private Button btn_bottommenu1;
	private Button btn_bottommenu2;
	private Button btn_bottommenu3;
	private Button btn_bottommenu4;
	private Button btn_bottommenu5;
	//title bar
	private TextView text_titlebar_text;
	private ImageButton btn_titlebar_left_btn1;
	private ImageButton btn_titlebar_right_btn1;
	private void initUI() {
		this.requestWindowFeature(Window.FEATURE_NO_TITLE);
		this.setContentView(R.layout.video_browser);
		//title bar
		text_titlebar_text = (TextView) findViewById(R.id.titlebar_text);
		text_titlebar_text.setText("Andzop");
		btn_titlebar_left_btn1 = (ImageButton) findViewById(R.id.titlebar_left_btn1);
		btn_titlebar_left_btn1.setBackgroundResource(R.drawable.btn_home);
		btn_titlebar_left_btn1.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				finish();
			}
		});
		btn_titlebar_right_btn1 = (ImageButton) findViewById(R.id.titlebar_right_btn1);
		btn_titlebar_right_btn1.setBackgroundResource(R.drawable.btn_help);
		btn_titlebar_right_btn1.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				showDialog(DIALOG_HELP);
			}
		});
		//bottom menu
		int l_btnWidth = this.getWindowManager().getDefaultDisplay().getWidth()/5;
		btn_bottommenu1 = (Button) findViewById(R.id.video_browser_btn1);
		//btn_bottommenu1 = (ActionMenuButton) findViewById(R.id.main_VideoBrowser2_btn1);
		btn_bottommenu1.setWidth(l_btnWidth);
		btn_bottommenu1.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				btn_bottommenu1.setEnabled(false);
				btn_bottommenu2.setEnabled(true);
				btn_bottommenu3.setEnabled(true); 
				currentFocusedBtn = 1;
				last_list_view_pos = 0;
				media_browser_load_option = 2;
				last_media_browser_load_option = media_browser_load_option;
				loadVideosFromDirectory("/sdcard/");
			}
		});
		btn_bottommenu2 = (Button) findViewById(R.id.video_browser_btn2);
		btn_bottommenu2.setWidth(l_btnWidth);
		btn_bottommenu2.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				btn_bottommenu1.setEnabled(true);
				btn_bottommenu2.setEnabled(false);
				btn_bottommenu3.setEnabled(true);
				currentFocusedBtn = 2;
				last_list_view_pos = 0;
				media_browser_load_option = 0;
				last_media_browser_load_option = media_browser_load_option;
				loadVideosFromDirectory("/sdcard/");
			}
		});
		btn_bottommenu3 = (Button) findViewById(R.id.video_browser_btn3);
		btn_bottommenu3.setWidth(l_btnWidth);
		btn_bottommenu3.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				btn_bottommenu1.setEnabled(true);
				btn_bottommenu2.setEnabled(true);
				btn_bottommenu3.setEnabled(false);
				currentFocusedBtn = 3;
				last_list_view_pos = 0;
				media_browser_load_option = 1;
				last_media_browser_load_option = media_browser_load_option;
				loadVideosFromDirectory("/sdcard/");
			}
		});
		btn_bottommenu4 = (Button) findViewById(R.id.video_browser_btn4);
		btn_bottommenu4.setWidth(l_btnWidth);
		btn_bottommenu4.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				//count the number of entries selected
				int l_selected = getNumOfEntriesSelected();
				if(l_selected == 0) {
					Toast.makeText(mContext, "No Video is Selected!", Toast.LENGTH_SHORT).show();
					return;
				}
				//if there're pictures selected, start playing
				playSelectedVideo();
			}
		});
		
		btn_bottommenu5 = (Button) findViewById(R.id.video_browser_btn5);
		btn_bottommenu5.setWidth(l_btnWidth);
		btn_bottommenu5.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				Toast.makeText(mContext, "deleting all txt files", Toast.LENGTH_LONG).show();
				//delete all dependency files
				FileUtilsStatic.deleteAllDepFiles(currentDirectory.getPath());
			}
		});
		
		media_browser_load_option = last_media_browser_load_option;
		if (media_browser_load_option==2) {
			btn_bottommenu1.setEnabled(false);
		} else if (media_browser_load_option==0) {
			btn_bottommenu2.setEnabled(false);
		} else if (media_browser_load_option==1){
			btn_bottommenu3.setEnabled(false);
		}
		loadVideosFromDirectory("/sdcard/");
	}
	
	private void playSelectedVideo() {
		Intent lAndzopIntent = new Intent();
		lAndzopIntent.setClass(mContext, feipeng.andzop.Main.VideoPlayer.class);
		ArrayList<String> lFullPathList = new ArrayList<String>();
		//TODO: get all selected videos
		int lSize = this.mSelected.size();
		for (int i = 0; i < lSize; ++i) {
			if (this.mSelected.get(i)) {
				lFullPathList.add(this.displayEntries.get(i).getText());
			}
		}
		lAndzopIntent.putExtra(pucVideoFileNameList, lFullPathList);
		startActivity(lAndzopIntent);
	}
	
	//refresh the UI when the directoryEntries changes
	private static int last_list_view_pos = 0;
	public void refreshUI() {
		int l_btnWidth = this.getWindowManager().getDefaultDisplay().getWidth()/5;
		btn_bottommenu1.setWidth(l_btnWidth);
		btn_bottommenu2.setWidth(l_btnWidth);
		btn_bottommenu3.setWidth(l_btnWidth);
		//btn_bottommenu4.setWidth(l_btnWidth);
		
		SlowAdapter itla = new SlowAdapter(this);
		itla.setListItems(this.displayEntries);		
		this.setListAdapter(itla);
        getListView().setOnScrollListener(this);
        int l_size = this.displayEntries.size();
        if (l_size > 50) {
        	getListView().setFastScrollEnabled(true);
        } else {
        	getListView().setFastScrollEnabled(false);
        }
        if (l_size > 0) {
	        if (last_list_view_pos < l_size) {
				getListView().setSelection(last_list_view_pos);
			} else {
				getListView().setSelection(l_size-1);
			}
        }
		registerForContextMenu(getListView());
	}
	
	@Override
    public void onConfigurationChanged (Configuration newConfig) {
    	super.onConfigurationChanged(newConfig);
    	refreshUI();
    }
	
	static final int DIALOG_LOAD_MEDIA = 1;
	static final int DIALOG_HELP = 2;
	@Override
	protected Dialog onCreateDialog(int id) {
        switch(id) {
        case DIALOG_LOAD_MEDIA:
        	ProgressDialog dialog = new ProgressDialog(this);
	        dialog.setTitle("Load Files");
	        dialog.setMessage("Please wait while loading...");
	        dialog.setIndeterminate(true);
	        dialog.setCancelable(true);
        	return dialog;
        case DIALOG_HELP:
        	SpannableStringBuilder str1 = new SpannableStringBuilder("");
    		
    		AlertDialog.Builder builder;
			AlertDialog alertDialog;
		
			Context mContext = VideoBrowser2.this;
			LayoutInflater inflater = (LayoutInflater) mContext.getSystemService(LAYOUT_INFLATER_SERVICE);
			View layout = inflater.inflate(R.layout.custom_dialog1,
			                               (ViewGroup) findViewById(R.id.custom_dialog1_layout_root));
		
			TextView text = (TextView) layout.findViewById(R.id.custom_dialog1_text);
			text.setTextSize(16);
			text.setText(str1, TextView.BufferType.SPANNABLE);
			
			Button btn = (Button) layout.findViewById(R.id.custom_dialog1__btn1);
			btn.setOnClickListener(new View.OnClickListener() {
				public void onClick(View v) {
					dismissDialog(DIALOG_HELP);
				}
			});
			builder = new AlertDialog.Builder(mContext);
			builder.setView(layout);
			alertDialog = builder.create();
			return alertDialog;
        default:
        	return null;
        }
	}
	/**
	 * scroll events methods: this part of the source code contain the control source code
	 * for handling scroll events
	 */
	private boolean mBusy = false;
	private void disableButtons() {
		btn_bottommenu1.setEnabled(false);
		btn_bottommenu2.setEnabled(false);
		btn_bottommenu3.setEnabled(false);
		btn_bottommenu4.setEnabled(false);
	}
	
	private void enableButtons() {
		if (currentFocusedBtn!=1) {
			btn_bottommenu1.setEnabled(true);
		}
		if (currentFocusedBtn!=2) {
			btn_bottommenu2.setEnabled(true);
		}
		if (currentFocusedBtn!=3) {
			btn_bottommenu3.setEnabled(true);
		}
		btn_bottommenu4.setEnabled(true);
	}
	public void onScroll(AbsListView view, int firstVisibleItem,
			int visibleItemCount, int totalItemCount) {
		last_list_view_pos = view.getFirstVisiblePosition();
	}

	//private boolean mSaveMemory = false;
	public void onScrollStateChanged(AbsListView view, int scrollState) {		
		switch (scrollState) {
        case OnScrollListener.SCROLL_STATE_IDLE:
        	enableButtons();
            mBusy = false;
            int first = view.getFirstVisiblePosition();
            int count = view.getChildCount();
            int l_releaseTarget;
            for (int i=0; i<count; i++) {
            	IconifiedTextSelectedView t = (IconifiedTextSelectedView)view.getChildAt(i);
            	if (t.getTag()!=null) {
            		String filename = this.displayEntries.get(first+i).getText();
            		t.setText(filename);
            		int l_type = this.displayEntries.get(first+i).getType();
            		Drawable l_icon = null;
            		try {
	            		if (l_type == 1) {
	            			l_icon = null;
	            		} else if (l_type == 2) {
	            			l_icon = null;
	            		}
            		} catch (OutOfMemoryError e) {
            			//if outofmemory, we try to clean up 10 view image resources,
            			//and try again
            			for (int j = 0; j < 10; ++j) {
            				l_releaseTarget = first - count - j;
	    	            	if (l_releaseTarget > 0) {
	    	    				IconifiedTextSelected l_its = displayEntries.get(l_releaseTarget);
	    	    				IconifiedTextSelectedView l_itsv = (IconifiedTextSelectedView)
	    	    					this.getListView().getChildAt(l_releaseTarget);
	    	    				if (l_itsv!=null) {
	    	    					l_itsv.setIcon(null);
	    	    				}
	    	    				if (l_its != null) {
	    	    					Drawable l_dr = l_its.getIcon();
	    	    					l_its.setIcon(null);
	    	    					if (l_dr != null) {
	    	    						l_dr.setCallback(null);
	    	    						l_dr = null;
	    	    					}
	    	    				}
	    	            	}
            			}
            			System.gc();
            			//after clean up, we try again
            			if (l_type == 1) {
	            			l_icon = null;
	            		} else if (l_type == 2) {
	            			l_icon = null;
	            		}
            		}
            		this.displayEntries.get(first+i).setIcon(l_icon);
            		if (l_icon != null) {
            			t.setIcon(l_icon);
            			t.setTag(null);
            		}
            	}
            }
            //System.gc();
            break;
        case OnScrollListener.SCROLL_STATE_TOUCH_SCROLL:
        	//disableButtons();
            mBusy = true;
            break;
        case OnScrollListener.SCROLL_STATE_FLING:
        	//disableButtons();
            mBusy = true;
            break;
        }
	}
	
	/**
	 * List item click action
	 */
	private File currentFile;
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		super.onListItemClick(l, v, position, id);
		last_list_view_pos = position;
		String selectedFileOrDirName = this.displayEntries.get((int)id).getText();
		if (selectedFileOrDirName.equals(upOneLevel)) {
			if (this.currentDirectory.getParent()!=null) {
				last_list_view_pos = 0;
				browseTo(this.currentDirectory.getParentFile());
			}
		} else {
			File l_clickedFile = new File(this.displayEntries.get((int)id).getText());
			if (l_clickedFile != null) {
				if (l_clickedFile.isDirectory()) {
					last_list_view_pos = 0;
					browseTo(l_clickedFile);
				} else {
					showContextMenuForFile(l_clickedFile, position);
				}
			}
		}
	}
	
	private void browseTo(final File _dir) {
		if (_dir.isDirectory()) {
			this.currentDirectory = _dir;
			loadVideosFromDirectory(_dir.getAbsolutePath());
		} 
	}
	
	private static final int CON_ANDZOP = 0;
	private static final int CON_VIEW = 1;
	private static final int CON_DELETE = 2;
	private static final int CON_BACK = 3;
	final CharSequence[] items = {
			"View with Andzop",
			"View the File",
			"Delete the File",
			"Back"};
	public static final String pucVideoFileNameList = "pucVideoFileName";
	private void showContextMenuForFile(final File _file, final int _pos) {
		currentFile = _file;
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle("Choose an Action");
		builder.setItems(items, new DialogInterface.OnClickListener() {
		    public void onClick(DialogInterface dialog, int item) {
		    	switch (item) {
		    	case CON_ANDZOP:
		    		Intent lAndzopIntent = new Intent();
		    		lAndzopIntent.setClass(mContext, feipeng.andzop.Main.VideoPlayer.class);
		    		ArrayList<String> lFullPathList = new ArrayList<String>();
		    		lFullPathList.add(_file.getAbsolutePath());
		    		lAndzopIntent.putExtra(pucVideoFileNameList, lFullPathList);
		    		startActivity(lAndzopIntent);
		    		break;
		    	case CON_VIEW:
		    		Uri data = Uri.parse("file://" + _file.getAbsolutePath());
		    		Intent l_intent = new Intent(android.content.Intent.ACTION_VIEW);
		    		if (checkEndsWithInStringArray(_file.getName(),
							getResources().getStringArray(R.array.fileEndingVideo))) {
		    			l_intent.setDataAndType(data, "video/*");
			    		try {
			    			startActivity(l_intent);
			    		} catch (Exception e) {
			    			Toast.makeText(mContext, "Sorry, Andzop cannot find an viewer for the file.", Toast.LENGTH_LONG);
			    		}
		    		}
		    		break;
		    	case CON_DELETE:
		    		OnClickListener yesButtonListener = new OnClickListener() {
		    			public void onClick(DialogInterface arg0, int arg1) {	
		    				_file.delete();
		    				VideoBrowser2.this.browseTo(VideoBrowser2.this.currentDirectory);
		    			}
		    		};
		    		OnClickListener noButtonListener = new OnClickListener() {
		    			public void onClick(DialogInterface arg0, int arg1) {
		    				//do nothing
		    			}
		    		};
		    		AlertDialog.Builder builder = new AlertDialog.Builder(VideoBrowser2.this);
		    		builder.setMessage("Are you sure you want to delete this file?")
		    		.setCancelable(false)
		    		.setPositiveButton("Yes", yesButtonListener)
		    		.setNegativeButton("No", noButtonListener);
		    		AlertDialog deleteFileAlert = builder.create();
		    		deleteFileAlert.show();
		    		return;
		    	case CON_BACK:
		    		return;
		    	}
		    }
		});
		AlertDialog contextDialog = builder.create();
		contextDialog.show();
	}
	
	public void updateNumOfSelected() {
		try {
			int lNumOfSelected = this.getNumOfEntriesSelected(); 
			if (text_titlebar_text != null) {
				CharSequence lMsg = text_titlebar_text.getText();
				int lIndex = lMsg.toString().lastIndexOf("-");
				lMsg = lMsg.subSequence(0, lIndex);
				text_titlebar_text.setText(lMsg + "- " + lNumOfSelected);
			}
		} catch (Exception e) {
			Log.e("updateNumOfSelected", e.toString());
		}
	}
	
	/**
	 * Slow adapter: this part of the code implements the list adapter
	 * Will not bind views while the list is scrolling
	 */
	private class SlowAdapter extends BaseAdapter {
    	/** Remember our context so we can use it when constructing views. */
    	private Context mContext;

    	private List<IconifiedTextSelected> mItems = new ArrayList<IconifiedTextSelected>();

    	public SlowAdapter(Context context) {
    		mContext = context;
    	}

    	public void setListItems(List<IconifiedTextSelected> lit) 
    	{ mItems = lit; }

    	/** @return The number of items in the */
    	public int getCount() { return mItems.size(); }

    	public Object getItem(int position) 
    	{ return mItems.get(position); }

    	/** Use the array index as a unique id. */
    	public long getItemId(int position) {
    		return position;
    	}

    	/** @param convertView The old view to overwrite, if one is passed
    	 * @returns a IconifiedTextSelectedView that holds wraps around an IconifiedText */
    	public View getView(int position, View convertView, ViewGroup parent) {
    		IconifiedTextSelectedView btv = null;
    		try {
	    		if (convertView == null) {
	    			btv = new IconifiedTextSelectedView(mContext, mItems.get(position));
	    		} else { // Reuse/Overwrite the View passed
	    			// We are assuming(!) that it is castable! 
	    			btv = (IconifiedTextSelectedView) convertView;
	    			btv.setText(mItems.get(position).getText());
	    		}
	    		//the select all icon
	//    		if (position==0) {
	//    			if (mItems.get(0).getIcon()!=null) {
	//    				btv.setIcon(mItems.get(position).getIcon());
	//    			} else {
	//	    			if (VideoBrowser2.self.media_browser_load_option==0) {
	//	    				btv.setIcon(R.drawable.folderback);
	//	    			} else {
	//	    				btv.setIcon(R.drawable.selectall);
	//	    			}
	//    			}
	//    		}
	    		//in busy mode
	    		if (mBusy){
	    			//if icon is NULL: the icon is not loaded yet; load default icon
	    			if (mItems.get(position).getIcon()==null) {
	    				btv.setIcon(R.drawable.video);
	    				//mark this view, indicates the icon is not loaded
	    				btv.setTag(this);
	    			} else {
	    				//if icon is not null, just display the icon
	    				btv.setIcon(mItems.get(position).getIcon());
	    				//mark this view, indicates the icon is loaded
	    				btv.setTag(null);
	    			}
	    		} else {
	    			//if not busy
	    			Drawable d = mItems.get(position).getIcon();
	    			if (d == null) {
	    				//icon is not loaded, load now
	    				btv.setIcon(R.drawable.video);
	    				btv.setTag(this);
	    			} else {
	    				btv.setIcon(mItems.get(position).getIcon());
	    				btv.setTag(null);
	    			}
	    		}
	    		if (mItems.get(position).getVisibility()) {
	    			btv.setVisibility(true);
	    		} else {
	    			btv.setVisibility(false);
	    		}
	    		btv.mCheckbox.setTag(position);
	    		btv.mCheckbox.setOnClickListener(new View.OnClickListener() {		
					public void onClick(View v) {
						int pos = (Integer)v.getTag();
						//select all change
	    					if (VideoBrowser2.mSelected.get(pos)==false) {
	    						VideoBrowser2.setEntrySelected(pos);
	    						updateNumOfSelected();
	    					} else {
	    						VideoBrowser2.setEntryUnselected(pos);
	    						updateNumOfSelected();
	    					}
	//    				}
					}
				});
	    		btv.mCheckbox.setChecked(VideoBrowser2.mSelected.get(position));
    		} catch (Exception e) {
    			Log.e("VideoBrowser2-getView", e.getLocalizedMessage());
    		}

    		return btv;
    	}
    }
}
