package com.zincland.powder;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.EditText;

public class PowderActivity extends Activity {
	
	private static final String TAG = "POWDER";
	
	public native int[] getFrameBufferJNI(int [] oldarr);
	public native int getFrameWidthJNI();	// Valid only after fetching frame buffer!
	public native int getFrameHeightJNI();
	
	public native boolean startPowderThreadJNI(String filepath);
	public native void vblJNI();
	public native void forceSaveJNI();
	
	public native void setStylusPosJNI(boolean state, int x, int y);
	public native int pollButtonReqJNI();
	public native void postInputStringJNI(String text);
	public native void postDirJNI(int dx, int dy);
	public native void setFakeButtonJNI(int button, boolean state);
	public native void postOrientationJNI(boolean isportrait);
	public native void revertDefaultsJNI();
	
	static
	{
		System.loadLibrary("powder-jni");
	}
	
	private WindowManager mWindowManager;
	private Display mDisplay;
	private PowderView mPowderView;
	
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mWindowManager = (WindowManager) getSystemService(WINDOW_SERVICE);
        mDisplay = mWindowManager.getDefaultDisplay();
        
        // Always axe the title, it is useless.
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        // We do not want to time out as we are a video game.
        // People may spend long times staring at their 
        // choice!
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        
        Log.v(TAG, "Display size " + mDisplay.getWidth() + " " + mDisplay.getHeight());
        boolean	isportrait = mDisplay.getWidth() < mDisplay.getHeight();
        
    	int 	orientation = getRequestedOrientation();
    	if (orientation != ActivityInfo.SCREEN_ORIENTATION_PORTRAIT &&
    		orientation != ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE)
    	{
    		// Force us into an orientation.  We have to guess one here based on the
    		// aspect even though this is wrong for the transformer.
    		if (isportrait)
    			setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
    		else
    			setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
    	}
    	
        if (!isportrait)
        {
        	getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);        	
        }
        
        mPowderView = new PowderView(this);
        mPowderView.setFocusableInTouchMode(true);
        
        setContentView(mPowderView);
    }
    
    @Override
    protected void onResume() {
    	super.onResume();
    	
    	Log.v(TAG, "Resume");
    	
        File pdir = getExternalFilesDir(null);
        
        final String pdirpath;
        if (pdir == null)
        {
        	// This is unlikely to work.  Should likely warn some how?
        	Log.v(TAG, "No path to a directory found");
        	pdirpath = "./";
        }
        else
        {
        	pdirpath = pdir.getAbsolutePath();
        	Log.v(TAG, "Storage at " + pdirpath);
        }
        
        // We post orientation first as it is just a global and this
        // ensures we have the right orientation for our first rebuild.
        postOrientationJNI(getRequestedOrientation() == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        
        Log.v(TAG, "Posted orientation");
		Log.v(TAG, "Fork to JNI");
		boolean reused = startPowderThreadJNI(pdirpath);
		Log.v(TAG, "Back from JNI! Reuse: " + String.valueOf(reused));
    }
    
    @Override
    protected void onPause() {
    	super.onPause();
    	
    	Log.v(TAG, "Pause");
    	
    	forceSaveJNI();
    	Log.v(TAG, "Saved");
    }
    
    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
    	MenuInflater	inflater = getMenuInflater();
    	
    	inflater.inflate(R.menu.optionsmenu, menu);
    	return true;
    }
    
    @Override
    public boolean onOptionsItemSelected(MenuItem item)
    {
    	switch (item.getItemId())
    	{
    		case R.id.rotateportrait:
    	    	int 	orientation = getRequestedOrientation();
    	    	if (orientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT)
    	    		setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
    	    	else
    	    		setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
    			return true;
    		case R.id.showcontrols:
    			mPowderView.toggleControls();
    			return true;
    		case R.id.showfps:
    			mPowderView.toggleShowFPS();
    			return true;
    		case R.id.yesrevertdefault:
    			revertDefaultsJNI();
    			return true;
    		case R.id.stretchscreen:
    			mPowderView.toggleStretchScreen();
    			return true;
    		case R.id.openfiles:
    		{
    	        File pdir = getExternalFilesDir(null);
    	        
    	        if (pdir != null)
    	        {
        	        AlertDialog.Builder alert = new AlertDialog.Builder(this);
        	        alert.setCancelable(true);
        	        alert.setTitle("View who's history?");
        	        alert.setPositiveButton("Cancel", null);
        	        final Context parentcontext = this;

        	        File[] filelist = pdir.listFiles();
        	        final ArrayList<File> txtfiles = new ArrayList<File>();
        	        
        	        for (int i = 0; i < filelist.length; i++)
        	        {
        	        	if (filelist[i].getName().endsWith(".txt"))
        	        	{
        	        		txtfiles.add(filelist[i]);
        	        	}
        	        }
        	        CharSequence[]  charnames = new String[txtfiles.size()];
        	        for (int i = 0; i < txtfiles.size(); i++)
        	        {
        	        	String name = txtfiles.get(i).getName();
        	        	// Since we filtered by .txt, we are safe to remove at .
        	        	name = name.substring(0, name.lastIndexOf('.'));
        	        	charnames[i] = name;
        	        }
        	        alert.setItems(charnames,  new DialogInterface.OnClickListener()
        	        {
        	        	public void onClick(DialogInterface dialog, int item)
        	        	{
        	        		Uri u = Uri.fromFile(txtfiles.get(item));
        	    			Intent intent = new Intent();
        	    			intent.setAction(Intent.ACTION_VIEW);
        	    			
            	        	intent.setDataAndType(u, "text/*");
            	        	
            	        	final PackageManager packageManager = getPackageManager();
            	        	List list = packageManager.queryIntentActivities(intent, PackageManager.MATCH_DEFAULT_ONLY);
            	        	
            	        	if (list.size() > 0)
            	        	{
            	        		startActivity(intent);
            	        		dialog.dismiss();
            	        	}
            	        	else
            	        	{
               	        		dialog.dismiss();
               	        	 	final AlertDialog.Builder alert = new AlertDialog.Builder(parentcontext);
        	    	    		alert.setCancelable(false);
        	    	    		alert.setMessage("No text viewer installed on this device.");
        	    	    		alert.setPositiveButton("Ok", null);
        	    	    		alert.show();
            	        	}		
        	        	}
        	        });
        	        
        	        if (txtfiles.size() > 0)
        	        {
        	        	alert.show();
        	        }
        	        else
        	        {
	    	    		final AlertDialog.Builder alert2 = new AlertDialog.Builder(this);
	    	    		alert2.setCancelable(false);
	    	    		alert2.setMessage("No character logs have been saved yet.");
	    	    		alert2.setPositiveButton("Ok", null);
	    	    		alert2.show();
    	        	}
    	        }
    	        else
    	        {
    	    		final AlertDialog.Builder alert = new AlertDialog.Builder(this);
    	    		alert.setCancelable(false);
    	    		alert.setMessage("No external storage configured for this device!");
    	    		alert.setPositiveButton("Ok", null);
    	    		alert.show();
	        	}
    			return true;
    		}
    		default:
    			return super.onOptionsItemSelected(item);
    	}
    }
    
    // Nothing brittle about this, nosiree!
    public class FakeButton
    {
    	public static final int UP = 0;
    	public static final int DOWN = 1;
    	public static final int LEFT = 2;
    	public static final int RIGHT = 3;
    	public static final int A = 4;
    	public static final int B = 5;
    	public static final int START = 6;
    	public static final int SELECT = 7;
    	public static final int R = 8;
    	public static final int L = 9;
    	public static final int X = 10;
    	public static final int Y = 11;
    	public static final int TOUCH = 12;
    	public static final int LID = 13;
    }
    
    public class Tile
    {
    	public int mX, mY;
    	public int mW, mH;
    	public Bitmap mBitmap;
    	public String mText;
    	public boolean mPressed;
    	public PowderView mView;
    	
    	public Tile(PowderView view, String text, int x, int y, int w, int h)
    	{
    		mView = view;
    		mX = x;
    		mY = y;
    		mW = w;
    		mH = h;
    		mPressed = false;
    		mText = text;
    		
    		rebuildLook();
    	}
    	
    	public void rebuildLook()
    	{
    		// Draw as pure text.
    		{
	    		mBitmap = Bitmap.createBitmap(mW, mH, Bitmap.Config.ARGB_8888);
	    		
	    		Canvas canvas = new Canvas(mBitmap);
	    		Paint bg = new Paint();
	    		bg.setARGB(96,255,255,255);
	    		RectF r = new RectF(0, 0, mW, mH);
	    		r.inset(2, 2);
	    		canvas.drawRoundRect(r, 4, 4, bg);
	    		bg.setARGB(255, 0, 0, 0);
	    		bg.setTextSize(mH/2);
	    		bg.setTextAlign(Paint.Align.CENTER);
	    		canvas.drawText(mText, mW/2, 3*(mH/4.0F), bg);
    		}
    		
    		// This hard-coded comparison is here to remind any readers
    		// of the vast gulf that exists between me and John Carmack.
    		if (mText == "H" || mText == "K" || mText == "J" || mText == "L")
    		{
    			Paint paint = new Paint();
    			paint.setARGB(255,255,255,255);
    			Bitmap piemap = BitmapFactory.decodeResource(mView.getResources(), R.drawable.pie);
    			Bitmap scaledpiemap = Bitmap.createScaledBitmap(piemap, mW, mH, true);
	    		Matrix rotmat = new Matrix();
	    		// Oh yeah, forgot Java sucks for a moment...
	    		// int anglelist[5] = { 0, 0, 270, 90, 180 };
	    		int angle = 0;
	    		if (mText == "H")
	    			angle = 0;
	    		else if (mText == "J")
	    			angle = 270;
	    		else if (mText == "K")
	    			angle = 90;
	    		else if (mText == "L")
	    			angle = 180;
	    		mBitmap = Bitmap.createBitmap(mW, mH, Bitmap.Config.ARGB_8888);
	    		Canvas canvas = new Canvas(mBitmap);
	    		rotmat.setRotate(angle, mW / 2, mH / 2);
	    		canvas.drawBitmap(scaledpiemap, rotmat, paint);
    		}
    	}
    	
    	public RectF getRect()
    	{
    		RectF r = new RectF(mX, mY, mX+mW, mY+mH);
    		return r;
    	}
    	
    	public void render(Canvas canvas)
    	{
    		if (mPressed)
    		{
    			Paint bg = new Paint();
    			bg.setARGB(255, 255, 128, 0);
    			RectF r = getRect();
    			r.inset(-1, -1);
    			canvas.drawRoundRect(r, 4, 4, bg);
    		}
    		canvas.drawBitmap(mBitmap, mX, mY, null);
    	}
    	
    	public void pressTile()
    	{
    		mPressed = true;
    	}
    	public void releaseTile()
    	{
    		// NOt necessarily a click!
    		mPressed = false;
    	}
    	
    	public void clickTile()
    	{
    	}

		public float hitDist(float px, float py) 
		{
			RectF	r = getRect();
			if (!r.contains(px, py))
				return 10000;
			
			float		cx = r.centerX();
			float 		cy = r.centerY();
			
			px -= cx;
			py -= cy;
			return (float) Math.sqrt(px * px + py * py);
		}
    }
    
    public class TileDir extends Tile
    {
    	public int mDX, mDY;
    	public TileDir(PowderView view, String text, int x, int y, int w, int h, int dx, int dy)
    	{
    		super(view, text, x, y, w, h);
    		mDX = dx;
    		mDY = dy;
    	}
    	
    	@Override
    	public void clickTile()
    	{
    		super.clickTile();
    		Log.v(TAG, "Post Direction " + String.valueOf(mDX) + ", " + String.valueOf(mDY));
			mView.mParent.postDirJNI(-1, 0);
    	}
    	
    }
    
    public class TileButton extends Tile
    {
    	public int mButton;
    	public TileButton(PowderView view, String text, int x, int y, int w, int h, int button)
    	{
    		super(view, text, x, y, w, h);
    		mButton = button;
    	}
    	
    	@Override
    	public void pressTile()
    	{
    		super.pressTile();
    		mView.mParent.setFakeButtonJNI(mButton, true);
    	}
    	@Override
    	public void releaseTile()
    	{
    		super.releaseTile();
    		mView.mParent.setFakeButtonJNI(mButton, false);
    	}
    	
    }
	
    class PowderView extends View {
    	public PowderActivity mParent;
    	private Rect mFrameScreen, mFrameGame;
    	private static final String TAG = "POWDER";
    	private long mLastCpuTime;
    	private long mLeftOverTime;
    	private int [] mOldFrameBuffer;
    	private Bitmap mFrameBitMap;
    	private boolean mShowControls;
    	private boolean mShowFPS;
    	private boolean mStretchScreen;
    	
    	private ArrayList<Tile>	mTileList;
    	private int mCurTile;
    	private boolean mTouchDown;
    	
    	public PowderView(PowderActivity context)
    	{
    		super(context);
    		
    		mParent = context;
    		mFrameScreen = null;
    		mFrameGame = null;   
    		mLastCpuTime = System.nanoTime();
    		mLeftOverTime = 0;
    		mOldFrameBuffer = new int[0];
    		mCurTile = -1;
    		mTouchDown = false;
    	             
    		mShowControls = true;
    		mShowFPS = false;
    		mTileList = new ArrayList<Tile>();
    		mStretchScreen = false;
    		
       	}
    	
    	public void buildAllTiles(int cw, int ch, int width)
    	{
    		int			w = width;
    		
    		// We lay vertically if landscape, horizontally if portrait.
    		// The idea is in portrait there is always a bottom strip available.
    		// In landscape, we run two strips on left and right to avoid overlap
    		// with the action bar.
    		if (cw > ch)
    		{
    			// Landscape!
    			// Not alphabetical, but having down and up reversed is just stupid.
    			int margin = width/2;
    			
    			if (false)
    			{
        			// Single column:
	        		mTileList.add(new TileButton(this, "H", margin, ch/2 - w*2, width, width, FakeButton.LEFT));
	        		mTileList.add(new TileButton(this, "K", margin, ch/2 - w, width, width, FakeButton.UP));
	        		mTileList.add(new TileButton(this, "J", margin, ch/2, width, width, FakeButton.DOWN));
	        		mTileList.add(new TileButton(this, "L", margin, ch/2+w, width, width, FakeButton.RIGHT));
    			}
    			else
    			{
    				// Rosette:  (<- why doesn't Eclipse recognize this as a word?)
    				mTileList.add(new TileButton(this, "H", margin-w/2, ch/2, width, width, FakeButton.LEFT));
	        		mTileList.add(new TileButton(this, "K", margin, ch/2 - w/2, width, width, FakeButton.UP));
	        		mTileList.add(new TileButton(this, "J", margin, ch/2 + w/2, width, width, FakeButton.DOWN));
	        		mTileList.add(new TileButton(this, "L", margin+w/2, ch/2, width, width, FakeButton.RIGHT));
    			}
	        		
        		mTileList.add(new TileButton(this, "B", cw - margin - width, ch/2 - w *2, width, width, FakeButton.B));
        		mTileList.add(new TileButton(this, "A", cw - margin - width, ch/2 + w, width, width, FakeButton.A));
    		}
    		else
    		{
    			// Portrait!
    			int  ypos = ch -  width - width/2;
        		mTileList.add(new TileButton(this, "H", 16, ypos, width, width, FakeButton.LEFT));
        		mTileList.add(new TileButton(this, "K", 16+1*w, ypos, width, width, FakeButton.UP));
           		mTileList.add(new TileButton(this, "J", 16+2*w, ypos, width, width, FakeButton.DOWN));
           		mTileList.add(new TileButton(this, "L", 16+3*w, ypos, width, width, FakeButton.RIGHT));
        		
        		mTileList.add(new TileButton(this, "B", 5*w, ypos, width, width, FakeButton.B));
        		mTileList.add(new TileButton(this, "A", 6*w, ypos, width, width, FakeButton.A));
    		}
    	}
    	
    	public void toggleControls()
    	{
    		mShowControls = !mShowControls;
    	}
    	public void toggleShowFPS()
    	{
    		mShowFPS = !mShowFPS;
    	}
    	public void toggleStretchScreen()
    	{
    		mStretchScreen = !mStretchScreen;
    	}
    	
    	public void queryName()
    	{
    		Context context = getContext();
    		final AlertDialog.Builder alert = new AlertDialog.Builder(context);
    		final EditText input = new EditText(context);
    		input.setSingleLine(true);
    		input.setFocusable(true);
    		
    		alert.setView(input);
    		alert.setCancelable(false);
    		alert.setMessage("Name?");
    		alert.setPositiveButton("Ok", new DialogInterface.OnClickListener()
    		{
    			public void onClick(DialogInterface dialog, int whichButton)
    			{
    				String value = input.getText().toString().trim();
    				mParent.postInputStringJNI(value);
    			}
    		});
    		
    		alert.show();
    	}
    	public void setStylusPos(float x, float y, boolean state)
    	{
    		if (mFrameScreen == null)
    			return;
    	
    		// Convert between spaces.
    		x -= mFrameScreen.left;
    		x /= mFrameScreen.width();
    		y -= mFrameScreen.top;
    		y /= mFrameScreen.height();
    		
    		x *= mFrameGame.width();
    		x += mFrameGame.left;
    		y *= mFrameGame.height();
    		y += mFrameGame.top;
    		
    		int ix = (int)x;
    		int iy = (int)y;
    		
    		// Now pass on
    		mParent.setStylusPosJNI(state, ix, iy);
    	}
    	
    	public void processButtonRequests()
    	{
    		int		bstate = mParent.pollButtonReqJNI();
    		
    		if (bstate == -1)
    			return;
    		
    		int		mode = bstate >> 16;
    		int		type = bstate & 0xffff;
    	
    		switch (mode)
    		{
    			case 7:
    			{
    				// Input request!
    				queryName();
    				break;
    			}
    		}
    	}
    	
    	public void renderTiles(Canvas canvas)
    	{
    		for (int i = 0; i < mTileList.size(); i++)
    		{
    			mTileList.get(i).render(canvas);
    		}
    	}
    	
    	@Override
		public boolean onTouchEvent(MotionEvent event)
    	{
    		float px = event.getX(0);
    		float py = event.getY(0);
    		
    		if ((event.getAction() & MotionEvent.ACTION_CANCEL) == MotionEvent.ACTION_CANCEL)
    		{
    			// Clear all hover status.
    			for (int i = 0; i < mTileList.size(); i++)
    			{
    				if (mTileList.get(i).mPressed)
    					mTileList.get(i).releaseTile();
    			}
      			mCurTile = -1;
      			mTouchDown = false;
    			// Pass down a stylus end.
    			setStylusPos(px, py, false);
    		}
    		else
    		{
    			// Always do one more down event.  If we also get an up, 
    			// we send a terminate.
    			// We need to terminate our series, but first put an extra down event so the
    			// last location is valid
    			if (!mTouchDown && mShowControls)
    			{
    				// Find which tile we pressed, if any.
    				// There may be multiple hits, in which
    				// case we pick the closest to the tile center.
    				mCurTile = -1;
    				float closesthit = 1000;
        			for (int i = 0; i < mTileList.size(); i++)
        			{
        				float	hitdist = mTileList.get(i).hitDist(px,  py);
        				if (hitdist < closesthit)
        				{
        					closesthit = hitdist;
        					mCurTile = i;
        				}
        			}
    			}
    			mTouchDown = true;
    			
    			if (mCurTile >= 0)
    			{
	    			if (mTileList.get(mCurTile).getRect().contains(px, py))
	    			{
	    				mTileList.get(mCurTile).pressTile();
	    			}
	    			else
	    				mTileList.get(mCurTile).releaseTile();
    			}
	    				
    			// In any case, never drop to lower level if we are on a button.
    			boolean overtile = false;
    			for (int i = 0; i < mTileList.size(); i++)
    			{
    				if (mTileList.get(i).getRect().contains(px, py))
    				{
    					overtile = true;
    				}
    			}
    			if (!mShowControls)
    				overtile = false;
    			
    			if (!overtile && mCurTile < 0)
    				setStylusPos(px, py, true);
    			
        		if ((event.getAction() & MotionEvent.ACTION_UP) == MotionEvent.ACTION_UP)
        		{
        			// Always let the lower layer know we are done.
        			setStylusPos(px, py, false);
        			
        			if (mCurTile >= 0 && mTileList.get(mCurTile).getRect().contains(px, py))
        			{
        				// Click in this tile!
        				mTileList.get(mCurTile).clickTile();
        			}
        			// In any case, press off!
          			for (int i = 0; i < mTileList.size(); i++)
        			{
        				if (mTileList.get(i).mPressed)
        					mTileList.get(i).releaseTile();
        			}
          			mCurTile = -1;
          			mTouchDown = false;
        		}
    		}
    		
    		invalidate();
    		return true;
    	}
    	
    	@Override
    	public boolean onKeyDown(int keyCode, KeyEvent event)
    	{
    		Log.v(TAG, "Key down " + keyCode);
    		switch (keyCode)
    		{
	    		case KeyEvent.KEYCODE_DPAD_LEFT:
	    		case KeyEvent.KEYCODE_H:
	    			Log.v(TAG, "Post left!");
	    			mParent.setFakeButtonJNI(FakeButton.LEFT, true);
	    			return true;
	    		case KeyEvent.KEYCODE_DPAD_RIGHT:
	    		case KeyEvent.KEYCODE_L:
	    			Log.v(TAG, "Post right!");
	    			mParent.setFakeButtonJNI(FakeButton.RIGHT, true);
	    			return true;
	    		case KeyEvent.KEYCODE_DPAD_UP:
	    		case KeyEvent.KEYCODE_K:
	    			Log.v(TAG, "Post up!");
	    			mParent.setFakeButtonJNI(FakeButton.UP, true);
	    			return true;
    			case KeyEvent.KEYCODE_DPAD_DOWN:
	    		case KeyEvent.KEYCODE_J:
	    			Log.v(TAG, "Post down!");
	    			mParent.setFakeButtonJNI(FakeButton.DOWN, true);
	    			return true;
	    		case KeyEvent.KEYCODE_SPACE:
	    			mParent.setFakeButtonJNI(FakeButton.A, true);
	    			return true;
    		}
    	
    		return false;
    	}
    	@Override
    	public boolean onKeyUp(int keyCode, KeyEvent event)
    	{
    		Log.v(TAG, "Key up " + keyCode);
    		switch (keyCode)
    		{
	    		case KeyEvent.KEYCODE_DPAD_LEFT:
	    		case KeyEvent.KEYCODE_H:
	    			Log.v(TAG, "Post left!");
	    			mParent.setFakeButtonJNI(FakeButton.LEFT, false);
	    			return true;
	    		case KeyEvent.KEYCODE_DPAD_RIGHT:
	    		case KeyEvent.KEYCODE_L:
	    			Log.v(TAG, "Post right!");
	    			mParent.setFakeButtonJNI(FakeButton.RIGHT, false);
	    			return true;
	    		case KeyEvent.KEYCODE_DPAD_UP:
	    		case KeyEvent.KEYCODE_K:
	    			Log.v(TAG, "Post up!");
	    			mParent.setFakeButtonJNI(FakeButton.UP, false);
	    			return true;
    			case KeyEvent.KEYCODE_DPAD_DOWN:
	    		case KeyEvent.KEYCODE_J:
	    			Log.v(TAG, "Post down!");
	    			mParent.setFakeButtonJNI(FakeButton.DOWN, false);
	    			return true;
	    		case KeyEvent.KEYCODE_SPACE:
	    			mParent.setFakeButtonJNI(FakeButton.A, false);
	    			return true;
    		}
    	
    		return false;
    	}
    	
    	protected void drawFrameTime(Canvas canvas, long frameticks, float x, float y)
    	{
    		Paint bg = new Paint();
    		double frametimems = frameticks / (1000.*1000.);
			int		framems = (int) frametimems;
			String text = String.valueOf(framems) + "ms";
    		
    		bg.setARGB(255, 0, 255, 128);
    		
    		canvas.drawText(text, x, y, bg);
    	}
    	
    	@Override
    	protected void onDraw(Canvas canvas)
    	{
    		// Send a series of vbls to catch up with our time delta
    		// Never send more than 5 (1/12th of second)
    		long		uiframeticks;
    		long		starttime, endtime;
    		{
    			long 	curtime = System.nanoTime();
    			long	elapsedtime = curtime - mLastCpuTime;
    			elapsedtime += mLeftOverTime;
    			float 	frametime = 1.0F/60.0F;
    			long	blankinterval = (long)(frametime * 1000.0F * 1000.0F * 1000.0F);
    			long	vblanks = elapsedtime / blankinterval;
    			if (vblanks > 5)
    			{
    				vblanks = 5;
    				mLeftOverTime = 0;
    			}
    			else
    			{
    				mLeftOverTime = elapsedtime % blankinterval;
    			}
    			
    			for (long i = 0; i < vblanks; i++)
    			{
        			mParent.vblJNI();
    			}
    		
    			uiframeticks = curtime - mLastCpuTime;
    			mLastCpuTime = curtime;
    		}
    		
    		// CLip our canvas to our canvas size.
    		// Seems rather silly to me.
    		int cwidth = canvas.getWidth();
			int cheight = canvas.getHeight();
			
			// If we haven't built our tiles, build them!
			if (mTileList.size() == 0)
			{
				int		dpiestimate;
				if (cwidth < cheight)
					dpiestimate = cwidth;
				else
					dpiestimate = cheight;
				// Find estimate of 8 pixel tile size.
				dpiestimate /= 30.0;
				// Want 4x4 tiles.
				dpiestimate *= 4;
				buildAllTiles(cwidth, cheight, dpiestimate);				
			}
    		
			canvas.clipRect(0, 0, cwidth, cheight);
			
    		Paint bg = new Paint();
    		canvas.drawColor(Color.BLACK);
    		
    		// Catch all updates
    		processButtonRequests();
    		
    		starttime = System.nanoTime();
    		
    		int[] colors = mParent.getFrameBufferJNI(mOldFrameBuffer);
    		
    		long fetchframebuffertime = System.nanoTime() - starttime;
    		
    		if (colors.length == 0)
    		{
        	}
    		else
    		{
        		mOldFrameBuffer = colors;
    		}
    		
    		int width = mParent.getFrameWidthJNI();
    		int height = mParent.getFrameHeightJNI();
    		
    		long createbitmaptime = 0;
    		long drawbitmaptime = 0;
    		// Verify we have a valid screen.
    		if (mOldFrameBuffer.length == width * height && width > 0)
    		{
    			// Center and find a scaling factor
    			int		scale = 2;
    			
    			while (width * scale <= cwidth && height * scale <= cheight)
    			{
    				scale++;
    			}
    			scale--;
    			
    			int window_center_x = cwidth/2;
    			int window_center_y = cheight/2;
    			int view_offset_x = - width * scale / 2;
    			int view_offset_y = - height * scale / 2;
    			
    			Rect r_dst;
    			
    			if (mStretchScreen)
    			{
    				// Additional scale to exactly fit.
    				float wscale = ((float)cwidth) / ((float) width);
    				float hscale = ((float)cheight) / ((float) height);
    				if (hscale < wscale)
    				{
    					// Match vertically.
        				r_dst = new Rect(window_center_x - (int)(width * hscale / 2),
        								 0,
        								 window_center_x + (int)(width * hscale / 2),
        								 cheight);
    				}
    				else
    				{
    					// Match horizontally
        				r_dst = new Rect(0,
        								 window_center_y - (int)(height * wscale / 2),
        								 cwidth,
        								 window_center_y + (int)(height* wscale / 2));
    				}
    			}
    			else
    			{
    				r_dst = new Rect(window_center_x + view_offset_x,
    									 window_center_y + view_offset_y,
    									 window_center_x + view_offset_x + width * scale,
    	    							 window_center_y + view_offset_y + height * scale);
    			}
    	    									 	
	    		mFrameGame = new Rect(0, 0, width, height);
	    		mFrameScreen = r_dst;
	    		starttime = System.nanoTime();
	    		if (mFrameBitMap == null || 
	    			mFrameBitMap.getWidth() != width || 
	    			mFrameBitMap.getHeight() != height)
	    		{
	    			mFrameBitMap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
	    		}
	    		mFrameBitMap.setPixels(mOldFrameBuffer, 0, width, 0, 0, width, height);
				createbitmaptime = System.nanoTime() - starttime;
				starttime = System.nanoTime();
				canvas.drawBitmap(mFrameBitMap, null, r_dst, bg);
				drawbitmaptime = System.nanoTime() - starttime;
	    	}

    		if (mShowControls)
    		{
    			renderTiles(canvas);
    		}
    		
    		// Print FPS
    		if (mShowFPS)
    		{
	    		drawFrameTime(canvas, uiframeticks, 10, 10);
	    		drawFrameTime(canvas, fetchframebuffertime, 10, 20);
	    		drawFrameTime(canvas, createbitmaptime, 10, 30);
	    		drawFrameTime(canvas, drawbitmaptime, 10, 40);
	    		drawFrameTime(canvas, System.nanoTime() - mLastCpuTime, 10, 50);
    		}
	    		
    		// Trigger another redraw
    		invalidate();
    	}
    }
}