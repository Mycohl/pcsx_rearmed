package net.dancingdemon.pcsx;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.os.Bundle;
import android.widget.Toast;
import android.view.Gravity;
import android.view.Display;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.ViewGroup;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.InputDevice;
import android.view.WindowManager;
import android.widget.TextView;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.FrameLayout;
import android.widget.AdapterView;
import android.widget.Adapter;
import android.widget.ArrayAdapter;
import android.content.SharedPreferences;
import android.content.Context;
import android.content.DialogInterface;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import android.preference.PreferenceFragment;
//import android.view.View.OnClickListener;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Typeface;
import android.util.Log;
import android.util.AttributeSet;
import android.util.Xml;
import java.io.File;
import java.io.FilenameFilter;
import java.util.Arrays;
import java.util.List;
import tv.ouya.console.api.OuyaController;

public class PCSX_ReARMed extends Activity implements SurfaceHolder.Callback, AdapterView.OnItemSelectedListener, View.OnKeyListener, DialogInterface.OnClickListener, DialogInterface.OnDismissListener
{
    private static final boolean DEBUG = false;

	private static final int MENU_NONE				= 0;
	private static final int MENU_MAIN				= 1;
	private static final int MENU_SETTINGS			= 2;
	private static final int MENU_SETTINGS_EMU		= 3;
	private static final int MENU_SETTINGS_VID		= 4;
	private static final int MENU_SETTINGS_AUD		= 5;
	private static final int MENU_SETTINGS_INP		= 6;
	private static final int MENU_SETTINGS_BIOS		= 7;
	private static final int MENU_SETTINGS_SAVE		= 8;
	private static final int MENU_SETTINGS_KEYBINDS	= 9;
	private static final int MENU_LOAD_GAME			= 10;
	private static final int MENU_CHANGE_DISC		= 11;

	private static final int BROWSE_RESULT_NONE		= 0;
	private static final int BROWSE_RESULT_GOTDIR	= 1;
	private static final int BROWSE_RESULT_GOTFILE	= 2;
	private static final int BROWSE_RESULT_DISMISSED	= 3;
	
	private static final int SUPPORTED_PLAYERS	= 2;
	
	private static final int OUYA_PAD_O		= 0;
	private static final int OUYA_PAD_U		= 1;
	private static final int OUYA_PAD_Y		= 2;
	private static final int OUYA_PAD_A		= 3;
	private static final int OUYA_PAD_UP		= 4;
	private static final int OUYA_PAD_DOWN	= 5;
	private static final int OUYA_PAD_LEFT	= 6;
	private static final int OUYA_PAD_RIGHT	= 7;
	private static final int OUYA_PAD_L1		= 8;
	private static final int OUYA_PAD_R1		= 9;
	private static final int OUYA_PAD_L2		= 10;
	private static final int OUYA_PAD_R2		= 11;
	private static final int OUYA_PAD_L3		= 12;
	private static final int OUYA_PAD_R3		= 13;
	private static final int OUYA_PAD_SELECT	= 14;
	private static final int OUYA_PAD_START	= 15;
	private static final int OUYA_AXIS_LX	= 16;
	private static final int OUYA_AXIS_LY	= 17;
	private static final int OUYA_AXIS_RX	= 18;
	private static final int OUYA_AXIS_RY	= 19;

	private static final int EMU_SELSTA_ENABLED		= 0;
	private static final int EMU_SELSTA_STARTED		= 1;
	private static final int EMU_SELSTA_SELDOWN		= 2;
	private static final int EMU_SELSTA_STADOWN		= 3;

	private static final int EMU_SELSTA_VWGROUP		= 0;
	private static final int EMU_SELSTA_VWSELECT		= 1;
	private static final int EMU_SELSTA_VWSTART		= 2;

	private static final int EMU_DPADHAT_UP			= 0;
	private static final int EMU_DPADHAT_DOWN		= 1;
	private static final int EMU_DPADHAT_LEFT		= 2;
	private static final int EMU_DPADHAT_RIGHT		= 3;

	private static boolean appFinished;

	private int[][] keyBinds;
	private boolean[][] keyPresses;
	private boolean[][] emuSelectStart;
	private View[][] emuSelectStartViews;
	private boolean[] padIsAnalog;

	private int showingMenu;
	private int lastShownMenu;
	private int lastShownSelection;
	private String browseBIOSPath, browseSavePath, browseROMPath;
	private boolean gameLoaded, exeLoaded;
	private int currentStateSlot;

	private static PCSX_ReARMed actObj;
    private static String TAG = "PCSX_ReARMed_Java";

	private String[] menuMainNoloadArray;
	private String[] menuMainLoadedArray;
	private String[] menuSettingsArray;

	private SurfaceView surfacev;
	private String loadErrorPath;
	private String pcsxMsgText;
	private TextView pcsxMsg;
	private LinearLayout menuScreen;
	private TextView menuTitle;
	private TextView menuSubtitle;
	private ListView menuList;
	private TextView menuSummary;
	
	private KeybindDialog kbd;
	
	public static Typeface gameFont;
	
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (DEBUG)
			Log.d(TAG, "onCreate()");
        
        setContentView(R.layout.main);
        surfacev = (SurfaceView)findViewById(R.id.surfaceview);
        surfacev.getHolder().addCallback(this);

		pcsxMsg = (TextView)findViewById(R.id.pcsxmsg);
		menuScreen = (LinearLayout)findViewById(R.id.menuscreen);
		menuTitle = (TextView)findViewById(R.id.menutitle);
		menuSubtitle = (TextView)findViewById(R.id.menusubtitle);
		menuList = (ListView)findViewById(R.id.menulist);
		menuList.setOnItemSelectedListener(this);
		menuList.setOnKeyListener(this);
		menuSummary = (TextView)findViewById(R.id.menusumm);

		PreferenceManager.setDefaultValues(this, R.xml.preferences, false);

		menuMainNoloadArray = new String[4];
		menuMainNoloadArray[0] = getString(R.string.menu_item_loadgame);
		menuMainNoloadArray[1] = getString(R.string.menu_item_bootbios);
		menuMainNoloadArray[2] = getString(R.string.menu_item_settings);
		menuMainNoloadArray[3] = getString(R.string.menu_item_exit);

		menuMainLoadedArray = new String[9];
		menuMainLoadedArray[0] = getString(R.string.menu_item_loadgame);
		menuMainLoadedArray[1] = getString(R.string.menu_item_resetgame);
		menuMainLoadedArray[2] = getString(R.string.menu_item_changedisc);
		menuMainLoadedArray[3] = getString(R.string.menu_item_savestate);
		menuMainLoadedArray[4] = getString(R.string.menu_item_loadstate);
		menuMainLoadedArray[5] = getString(R.string.menu_item_cheats);
		menuMainLoadedArray[6] = getString(R.string.menu_item_bootbios);
		menuMainLoadedArray[7] = getString(R.string.menu_item_settings);
		menuMainLoadedArray[8] = getString(R.string.menu_item_exit);

		menuSettingsArray = new String[4];
		menuSettingsArray[0] = getString(R.string.prefcat_emu);
		menuSettingsArray[1] = getString(R.string.prefcat_vid); 
		menuSettingsArray[2] = getString(R.string.prefcat_aud);
		menuSettingsArray[3] = getString(R.string.prefcat_inp);
		
		gameLoaded = false;
		
		actObj = this;
		OuyaController.init(this);
		OuyaController.showCursor(false); // hide the mouse cursor
		
		if (true) { // test for supported languages here
			gameFont = Typeface.createFromAsset(getAssets(), "GameFont.ttf");
			pcsxMsg.setTypeface(gameFont);
			menuTitle.setTypeface(gameFont);
			menuSubtitle.setTypeface(gameFont);
			menuSummary.setTypeface(gameFont);
		} else { // unsupported language
			gameFont = Typeface.MONOSPACE;
		}
		
		currentStateSlot = 0;
		
		kbd = null;
		
		appFinished = false;
    }

    @Override
    protected void onStart() {
        super.onStart();
        if (DEBUG)
			Log.d(TAG, "onStart()");

        surfacev.setVisibility(View.VISIBLE);
        nativeOnStart();

		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		if (prefs.getString("pref_maintain_psx_aspect_ratio", "").equals("on")) {
			setSurfaceAspectRatio(true);
		}
		
		setDefaultKeybinds();
		setKeypresses(null);
		setEmuSelectStart();
		setPadIsAnalog();
		
        displayMenu(MENU_MAIN, 0);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (DEBUG)
			Log.d(TAG, "onResume()");

        nativeOnResume();
        if (showingMenu == MENU_NONE) {
			nativeEmuPauseUnpause(false);
		}

    }
    
    @Override
    protected void onPause() {
        super.onPause();
        if (DEBUG)
			Log.d(TAG, "onPause()");

        nativeOnPause();
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (DEBUG)
			Log.d(TAG, "onStop()");

		appFinished = true;

        if (DEBUG)
			Log.d(TAG, "removing static objects");
		pcsxMsg = null;
		gameFont = null;
		actObj = null;
		menuMainNoloadArray = null;
		menuMainLoadedArray = null;
		menuSettingsArray = null;
		keyBinds = null;
		keyPresses = null;
		emuSelectStart = null;
		emuSelectStartViews = null;
		padIsAnalog = null;
		

        if (DEBUG)
			Log.d(TAG, "hiding views");
		surfacev.setVisibility(View.GONE);
		menuTitle.setVisibility(View.GONE);
		menuSubtitle.setVisibility(View.GONE);
		menuList.setVisibility(View.GONE);
		menuList.setAdapter(null);
		menuSummary.setVisibility(View.GONE);
		menuScreen.setVisibility(View.GONE);

        if (DEBUG)
			Log.d(TAG, "removing callbacks");
        surfacev.getHolder().getSurface().release();
        surfacev.getHolder().removeCallback(this);
		menuList.setOnItemSelectedListener(null);
		menuList.setOnKeyListener(null);

        nativeOnStop();

		WindowManager wm = getWindow().getWindowManager();
        //~ wm.removeViewImmediate(surfacev);
		surfacev = null;
        //~ wm.removeViewImmediate(menuTitle);
		menuTitle = null;
        //~ wm.removeViewImmediate(menuSubtitle);
		menuSubtitle = null;
        //~ wm.removeViewImmediate(menuList);
		menuList = null;
		menuSummary = null;
        //~ wm.removeViewImmediate(menuSummary);
		menuScreen = null;
        wm.removeViewImmediate(getWindow().getDecorView().findViewById(android.R.id.content).getRootView());

		System.gc();

    }
    
    public void exitApp() {
		finish();
		onStop();
		onDestroy();
		//~ System.exit(0);
	}

	public int browseForROM(int keyCode, StringBuilder path) {
		if (keyCode == OuyaController.BUTTON_O || keyCode == KeyEvent.KEYCODE_ENTER) {
			// open dir or file
			File file = new File(browseROMPath + (String)menuList.getSelectedItem());
			if (file.isDirectory()) {
				String[] filelist = file.list();
				if (filelist == null) {
					Toast.makeText(this, R.string.menu_browse_noacc, Toast.LENGTH_SHORT).show();
					return BROWSE_RESULT_NONE;
				}
				Arrays.sort(filelist);
				browseROMPath = file.getAbsolutePath();
				if (!browseROMPath.endsWith("/"))
					browseROMPath = browseROMPath + "/";
				menuSubtitle.setText(browseROMPath);
				menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, filelist));
				menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
				return BROWSE_RESULT_NONE;
			} else {
				if (!file.canRead()) {
					Toast.makeText(this, R.string.menu_browse_noacc, Toast.LENGTH_SHORT).show();
					return BROWSE_RESULT_NONE;
				}
				String ext = file.getName().substring(file.getName().lastIndexOf('.') + 1).toUpperCase();
				if (!ext.equals("CUE") && !ext.equals("BIN") && !ext.equals("IMG") && !ext.equals("ISO") &&
					!ext.equals("MDF") && !ext.equals("Z") && !ext.equals("BZ") && !ext.equals("ZNX") &&
					!ext.equals("EXE")) {
					Toast.makeText(this, R.string.menu_browse_file_wrongext, Toast.LENGTH_SHORT).show();
					return BROWSE_RESULT_NONE;
				}
				SharedPreferences.Editor prefEdit = PreferenceManager.getDefaultSharedPreferences(this).edit();
				prefEdit.putString("pref_pcsx_rearmed_rom_dir", browseROMPath);
				prefEdit.apply();
				path.append(file.getAbsolutePath());
				return BROWSE_RESULT_GOTFILE;
			}
		} else if (keyCode == OuyaController.BUTTON_U || keyCode == KeyEvent.KEYCODE_DEL) {
			// back dir
			if (browseROMPath.equals("/")) {
				return BROWSE_RESULT_NONE;
			}
			File dir = new File(browseROMPath);
			dir = dir.getParentFile();
			String[] filelist = dir.list();
			Arrays.sort(filelist);
			browseROMPath = dir.getAbsolutePath();
			if (!browseROMPath.endsWith("/"))
				browseROMPath = browseROMPath + "/";
			menuSubtitle.setText(browseROMPath);
			menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, filelist));
			menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
			return BROWSE_RESULT_NONE;
		//~ } else if (keyCode == OuyaController.BUTTON_Y || keyCode == KeyEvent.KEYCODE_S) {
			//~ return BROWSE_RESULT_GOTFILE;
		} else if (keyCode == OuyaController.BUTTON_A || keyCode == KeyEvent.KEYCODE_ESCAPE || keyCode == KeyEvent.KEYCODE_MENU) {
			return BROWSE_RESULT_DISMISSED;
		}
		return BROWSE_RESULT_NONE;
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if (appFinished) {
			if (DEBUG)
				Log.d(TAG, "Got KeyEvent after appFinished: " + event.toString());
			return false;
		}

		StringBuilder path;
		int result;
		long id;
        //~ if (DEBUG)
		//~ 	Log.d("KEYDOWN", "Got key: " + KeyEvent.keyCodeToString(keyCode));

		switch (showingMenu) {
			case MENU_NONE:
				if (keyCode == KeyEvent.KEYCODE_MENU || keyCode == KeyEvent.KEYCODE_ESCAPE) {
					nativeEmuPauseUnpause(true);
					displayMenu(lastShownMenu, lastShownSelection);
					return true;
				}
				setKeypresses(event);
				handleEmuMenu(event);
				return true;
				//~ return OuyaController.onKeyDown(keyCode, event) || super.onKeyDown(keyCode, event);
			case MENU_MAIN:
				id = menuList.getSelectedItemId();
				if (keyCode == OuyaController.BUTTON_O || keyCode == KeyEvent.KEYCODE_ENTER) {
					if (gameLoaded) {
						if (id == 0) {
							// load game
							displayMenu(MENU_LOAD_GAME);
						} else if (id == 1) {
							// reset game
							lastShownMenu = MENU_MAIN;
							lastShownSelection = (int)id;
							displayMenu(MENU_NONE);
							nativeEmuResetGame();
						} else if (id == 2) {
							// change disc
							displayMenu(MENU_CHANGE_DISC);
						} else if (id == 3) {
							// save state
							lastShownMenu = MENU_MAIN;
							lastShownSelection = (int)id;
							displayMenu(MENU_NONE);
							nativeEmuSaveLoad(true, currentStateSlot);
						} else if (id == 4) {
							// load state
							lastShownMenu = MENU_MAIN;
							displayMenu(MENU_NONE);
							lastShownSelection = (int)id;
							nativeEmuSaveLoad(false, currentStateSlot);
						} else if (id == 5) {
							// cheats
							Toast.makeText(this, R.string.string_coming_soon, Toast.LENGTH_SHORT).show();
						} else if (id == 6) {
							// boot bios
							gameLoaded = true;
							lastShownMenu = MENU_MAIN;
							lastShownSelection = 6;
							displayMenu(MENU_NONE);
							nativeEmuLoadGame("BIOS");
						} else if (id == 7) {
							displayMenu(MENU_SETTINGS);
						} else if (id == 8) {
							exitApp();
							return true;
						} else {
							Toast.makeText(this, "Not done yet.", Toast.LENGTH_SHORT).show();
						}
					} else {
						if (id == 0) {
							// load game
							displayMenu(MENU_LOAD_GAME);
						} else if (id == 1) {
							// boot bios
							gameLoaded = true;
							lastShownMenu = MENU_MAIN;
							lastShownSelection = 6;
							displayMenu(MENU_NONE);
							nativeEmuLoadGame("BIOS");
						} else if (id == 2) {
							displayMenu(MENU_SETTINGS);
						} else if (id == 3) {
							exitApp();
							return true;
						} else {
							Toast.makeText(this, "Not done yet.", Toast.LENGTH_SHORT).show();
						}
					}
					return true;
				} else if (keyCode == OuyaController.BUTTON_A || keyCode == KeyEvent.KEYCODE_MENU || keyCode == KeyEvent.KEYCODE_ESCAPE) {
					if (gameLoaded) {
						lastShownMenu = MENU_MAIN;
						lastShownSelection = (int)id;
						displayMenu(MENU_NONE);
						nativeEmuPauseUnpause(false);
					}
					return true;
				} else if (keyCode == OuyaController.BUTTON_R1 || keyCode== KeyEvent.KEYCODE_PLUS || keyCode== KeyEvent.KEYCODE_EQUALS) {
					if (gameLoaded && (id == 3 || id == 4)) {
						currentStateSlot++;
						if (currentStateSlot > 9)
							currentStateSlot = 0;
						nativeSetMessage(getString(R.string.menu_savestate_selected) + " "  + currentStateSlot, 60);
					}
					return true;
				} else if (keyCode == OuyaController.BUTTON_L1 || keyCode== KeyEvent.KEYCODE_MINUS) {
					if (gameLoaded && (id == 3 || id == 4)) {
						currentStateSlot--;
						if (currentStateSlot < 0)
							currentStateSlot = 9;
						nativeSetMessage(getString(R.string.menu_savestate_selected) + " " + currentStateSlot, 60);
					}
					return true;
				}
				break;
			case MENU_SETTINGS:
				id = menuList.getSelectedItemId();
				if (keyCode == OuyaController.BUTTON_O || keyCode == KeyEvent.KEYCODE_ENTER) {
					if (id == 0)
						displayMenu(MENU_SETTINGS_EMU);
					else if (id == 1)
						displayMenu(MENU_SETTINGS_VID);
					else if (id == 2)
						displayMenu(MENU_SETTINGS_AUD);
					else if (id == 3)
						displayMenu(MENU_SETTINGS_INP);
					else
						Toast.makeText(this, "Not done yet.", Toast.LENGTH_SHORT).show();
					return true;
				} else if (keyCode == OuyaController.BUTTON_A || keyCode == KeyEvent.KEYCODE_ESCAPE) {
					displayMenu(MENU_MAIN, (gameLoaded) ? 7 : 2);
					return true;
				} else if (keyCode == KeyEvent.KEYCODE_MENU) {
					if (gameLoaded) {
						lastShownMenu = MENU_SETTINGS;
						lastShownSelection = (int)id;
						displayMenu(MENU_NONE);
						nativeEmuPauseUnpause(false);
					}
					return true;
				}
				break;
			case MENU_SETTINGS_EMU:
				if (keyCode == OuyaController.BUTTON_O || keyCode == KeyEvent.KEYCODE_ENTER) {
					showSettingsForMenu();
					return true;
				} else if (keyCode == OuyaController.BUTTON_A || keyCode == KeyEvent.KEYCODE_ESCAPE) {
					displayMenu(MENU_SETTINGS, 0);
					return true;
				} else if (keyCode == KeyEvent.KEYCODE_MENU) {
					if (gameLoaded) {
						lastShownMenu = MENU_SETTINGS_EMU;
						lastShownSelection = (int)menuList.getSelectedItemId();
						displayMenu(MENU_NONE);
						nativeEmuPauseUnpause(false);
					}
					return true;
				}
				break;
			case MENU_SETTINGS_VID:
				if (keyCode == OuyaController.BUTTON_O || keyCode == KeyEvent.KEYCODE_ENTER) {
					showSettingsForMenu();
					return true;
				} else if (keyCode == OuyaController.BUTTON_A || keyCode == KeyEvent.KEYCODE_ESCAPE) {
					displayMenu(MENU_SETTINGS, 1);
					return true;
				} else if (keyCode == KeyEvent.KEYCODE_MENU) {
					if (gameLoaded) {
						lastShownMenu = MENU_SETTINGS_VID;
						lastShownSelection = (int)menuList.getSelectedItemId();
						displayMenu(MENU_NONE);
						nativeEmuPauseUnpause(false);
					}
					return true;
				}
				break;
			case MENU_SETTINGS_AUD:
				if (keyCode == OuyaController.BUTTON_O || keyCode == KeyEvent.KEYCODE_ENTER) {
					showSettingsForMenu();
					return true;
				} else if (keyCode == OuyaController.BUTTON_A || keyCode == KeyEvent.KEYCODE_ESCAPE) {
					displayMenu(MENU_SETTINGS, 2);
					return true;
				} else if (keyCode == OuyaController.BUTTON_MENU) {
					if (gameLoaded) {
						lastShownMenu = MENU_SETTINGS_AUD;
						lastShownSelection = (int)menuList.getSelectedItemId();
						displayMenu(MENU_NONE);
						nativeEmuPauseUnpause(false);
					}
					return true;
				}
				break;
			case MENU_SETTINGS_INP:
				if (keyCode == OuyaController.BUTTON_O || keyCode == KeyEvent.KEYCODE_ENTER) {
					showSettingsForMenu();
					return true;
				} else if (keyCode == OuyaController.BUTTON_A || keyCode == KeyEvent.KEYCODE_ESCAPE) {
					displayMenu(MENU_SETTINGS, 3);
					return true;
				} else if (keyCode == KeyEvent.KEYCODE_MENU) {
					if (gameLoaded) {
						lastShownMenu = MENU_SETTINGS_INP;
						lastShownSelection = (int)menuList.getSelectedItemId();
						displayMenu(MENU_NONE);
						nativeEmuPauseUnpause(false);
					}
					return true;
				}
				break;
			case MENU_SETTINGS_BIOS:
				if (keyCode == OuyaController.BUTTON_O || keyCode == KeyEvent.KEYCODE_ENTER) {
					// open dir
					//~ File dir = new File(browseBIOSPath + ((TextView)menuList.getSelectedItem()).getText());
					File dir = new File(browseBIOSPath + (String)menuList.getSelectedItem());
					if (!dir.isDirectory())
						break;
					String[] filelist = dir.list();
					if (filelist == null) {
						Toast.makeText(this, R.string.menu_browse_noacc, Toast.LENGTH_SHORT).show();
						break;
					}
					Arrays.sort(filelist);
					browseBIOSPath = dir.getAbsolutePath();
					if (!browseBIOSPath.endsWith("/"))
						browseBIOSPath = browseBIOSPath + "/";
					menuSubtitle.setText(browseBIOSPath);
					menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, filelist));
					menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
					return true;
				} else if (keyCode == OuyaController.BUTTON_U || keyCode == KeyEvent.KEYCODE_DEL) {
					// back dir
					if (browseBIOSPath.equals("/")) {
						break;
					}
					File dir = new File(browseBIOSPath);
					dir = dir.getParentFile();
					browseBIOSPath = dir.getAbsolutePath();
					if (!browseBIOSPath.endsWith("/"))
						browseBIOSPath = browseBIOSPath + "/";
					menuSubtitle.setText(browseBIOSPath);
					String[] filelist = dir.list();
					Arrays.sort(filelist);
					menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, filelist));
					menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
					return true;
				} else if (keyCode == OuyaController.BUTTON_Y || keyCode == KeyEvent.KEYCODE_S) {
					// accept dir
					SharedPreferences.Editor prefEdit = PreferenceManager.getDefaultSharedPreferences(this).edit();
					prefEdit.putString("pref_pcsx_rearmed_bios_dir", browseBIOSPath);
					prefEdit.apply();
					displayMenu(MENU_SETTINGS_EMU, 2);
					return true;
				} else if (keyCode == OuyaController.BUTTON_A || keyCode == KeyEvent.KEYCODE_ESCAPE) {
					displayMenu(MENU_SETTINGS_EMU, 2);
					return true;
				} else if (keyCode == KeyEvent.KEYCODE_MENU) {
					if (gameLoaded) {
						lastShownMenu = MENU_SETTINGS_EMU;
						lastShownSelection = 2;
						displayMenu(MENU_NONE);
						nativeEmuPauseUnpause(false);
					}
					return true;
				}
				break;
			case MENU_SETTINGS_SAVE:
				if (keyCode == OuyaController.BUTTON_O || keyCode == KeyEvent.KEYCODE_ENTER) {
					// open dir
					File dir = new File(browseSavePath + (String)menuList.getSelectedItem());
					if (!dir.isDirectory())
						break;
					String[] filelist = dir.list();
					if (filelist == null) {
						Toast.makeText(this, R.string.menu_browse_noacc, Toast.LENGTH_SHORT).show();
						break;
					}
					Arrays.sort(filelist);
					browseSavePath = dir.getAbsolutePath();
					if (!browseSavePath.endsWith("/"))
						browseSavePath = browseSavePath + "/";
					menuSubtitle.setText(browseSavePath);
					menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, filelist));
					menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
					return true;
				} else if (keyCode == OuyaController.BUTTON_U || keyCode == KeyEvent.KEYCODE_DEL) {
					// back dir
					if (browseSavePath.equals("/")) {
						break;
					}
					File dir = new File(browseSavePath);
					dir = dir.getParentFile();
					String[] filelist = dir.list();
					Arrays.sort(filelist);
					browseSavePath = dir.getAbsolutePath();
					if (!browseSavePath.endsWith("/"))
						browseSavePath = browseSavePath + "/";
					menuSubtitle.setText(browseSavePath);
					menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, filelist));
					menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
					return true;
				} else if (keyCode == OuyaController.BUTTON_Y || keyCode == KeyEvent.KEYCODE_S) {
					// accept dir
					SharedPreferences.Editor prefEdit = PreferenceManager.getDefaultSharedPreferences(this).edit();
					prefEdit.putString("pref_pcsx_rearmed_save_dir", browseSavePath);
					prefEdit.apply();
					displayMenu(MENU_SETTINGS_EMU, 4);
					return true;
				} else if (keyCode == OuyaController.BUTTON_A || keyCode == KeyEvent.KEYCODE_ESCAPE) {
					displayMenu(MENU_SETTINGS_EMU, 4);
					return true;
				} else if (keyCode == KeyEvent.KEYCODE_MENU) {
					if (gameLoaded) {
						lastShownMenu = MENU_SETTINGS_EMU;
						lastShownSelection = 4;
						displayMenu(MENU_NONE);
						nativeEmuPauseUnpause(false);
					}
					return true;
				}
				break;
			case MENU_SETTINGS_KEYBINDS:
				if (keyCode == OuyaController.BUTTON_O || keyCode == KeyEvent.KEYCODE_ENTER) {
					id = menuList.getSelectedItemId();
					showKeybindDialog(id);
					return true;
				} else if (keyCode == OuyaController.BUTTON_Y || keyCode == KeyEvent.KEYCODE_DEL) {
					id = menuList.getSelectedItemId();
					resetKeybind(id);
	
					View c = menuList.getSelectedView();
					int top = (c == null) ? 0 : (c.getTop() - menuList.getPaddingTop());
					displayMenu(MENU_SETTINGS_KEYBINDS);
					menuList.setSelectionFromTop((int)id, top);

					return true;
				} else if (keyCode == OuyaController.BUTTON_A || keyCode == KeyEvent.KEYCODE_ESCAPE) {
					displayMenu(MENU_SETTINGS_INP, 4);
					return true;
				} else if (keyCode == KeyEvent.KEYCODE_MENU) {
					if (gameLoaded) {
						lastShownMenu = MENU_SETTINGS_INP;
						lastShownSelection = 4;
						displayMenu(MENU_NONE);
						nativeEmuPauseUnpause(false);
					}
					return true;
				}
				break;
			case MENU_LOAD_GAME:
				path = new StringBuilder();
				result = browseForROM(keyCode, path);
				if (result == BROWSE_RESULT_GOTFILE) {
					if (path.length() == 0) {
						displayMenu(MENU_MAIN, 0);
					} else {
						if (gameLoaded)
							nativeEmuUnloadGame();
						gameLoaded = true;
						lastShownMenu = MENU_MAIN;
						lastShownSelection = 0;
						displayMenu(MENU_NONE);
						nativeEmuLoadGame(path.toString());
					}
					return true;
				} else if (result == BROWSE_RESULT_DISMISSED) {
					if (keyCode == OuyaController.BUTTON_A || keyCode == KeyEvent.KEYCODE_ESCAPE)
						displayMenu(MENU_MAIN, 0);
					else {
						if (gameLoaded) {
							lastShownMenu = MENU_MAIN;
							lastShownSelection = 0;
							displayMenu(MENU_NONE);
							nativeEmuPauseUnpause(false);
						}
					}
					return true;
				}
				break;
			case MENU_CHANGE_DISC:
				path = new StringBuilder();
				result = browseForROM(keyCode, path);
				if (result == BROWSE_RESULT_GOTFILE) {
					nativeEmuChangeDisc(path.toString());
					lastShownMenu = MENU_MAIN;
					lastShownSelection = 2;
					displayMenu(MENU_NONE);
					nativeEmuPauseUnpause(false);
					return true;
				} else if (result == BROWSE_RESULT_DISMISSED) {
					if (keyCode == OuyaController.BUTTON_A || keyCode == KeyEvent.KEYCODE_ESCAPE)
						displayMenu(MENU_MAIN, 2);
					else {
						lastShownMenu = MENU_MAIN;
						lastShownSelection = 2;
						displayMenu(MENU_NONE);
						nativeEmuPauseUnpause(false);
					}
					return true;
				}
				break;
			default:
				return false;
		}
		//if (keyCode == KeyEvent.KEYCODE_ESCAPE)
		return false;
	}

	@Override
	public boolean onKeyUp(int keyCode, KeyEvent event) {
		if (appFinished) {
			if (DEBUG)
				Log.d(TAG, "Got KeyEvent after appFinished: " + event.toString());
			return false;
		}

		if (showingMenu == MENU_NONE) {
			setKeypresses(event);
			//~ return OuyaController.onKeyUp(keyCode, event) || super.onKeyUp(keyCode, event);
		}
		return true;
	}

	@Override
	public boolean onGenericMotionEvent(MotionEvent event) {
		if (appFinished) {
			if (DEBUG)
				Log.d(TAG, "Got MotionEvent after appFinished: " + event.toString());
			return false;
		}

		if((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0) {

			if ((showingMenu == MENU_NONE)) {
				// allow OUYA to spoof dpad for hat axis
				if (event.getAxisValue(MotionEvent.AXIS_HAT_X) != 0.0 || event.getAxisValue(MotionEvent.AXIS_HAT_Y) != 0.0)
					return false;

				int player = OuyaController.getPlayerNumByDeviceId(event.getDeviceId());
				
				// check for odd OUYA error
				if (player < 0 || player > 3) {
					if (DEBUG)
						Log.e(TAG, "onGenericMotionEvent() - Invalid player: " + player);
					Toast.makeText(this, R.string.error_invalid_playerid_controller, Toast.LENGTH_SHORT).show();
					return false;
				} else if (player >= SUPPORTED_PLAYERS) {
					return false;
				} else if (!padIsAnalog[player]) {
					// allow OUYA to spoof dpad for digital pad type during game play
					if (event.getAxisValue(MotionEvent.AXIS_X) != 0.0 || event.getAxisValue(MotionEvent.AXIS_Y) != 0.0)
						return false;
				}
			} else {
				// allow OUYA to spoof dpad in menu
				return false;
			}
			//~ return OuyaController.onGenericMotionEvent(event) || super.onGenericMotionEvent(event);
			return true;
		}
		else if((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0) {
			if (showingMenu == MENU_NONE) {
				handleEmuSelectStartMotion(event);
			}
		} else {
			if (DEBUG)
				Log.d(TAG, "Unknown input source class: " + event.getSource());
		}
		return true;
	}

	@Override
	public boolean onTouchEvent(MotionEvent event) {
		if (appFinished) {
			if (DEBUG)
				Log.d(TAG, "Got MotionEvent after appFinished: " + event.toString());
			return false;
		}

		if (showingMenu == MENU_NONE && event.getAction() == MotionEvent.ACTION_UP)
			handleEmuSelectStartTouch(event);

		// sometimes the cursor becomes visible again (pad restarted)
		OuyaController.showCursor(false);

		// menuList loses selection when taps happen >:-(
		if (menuList.getCount() > lastShownSelection)
			menuList.setSelection(lastShownSelection);
		else
			menuList.setSelection(0);

		return true;
	}

	@Override
	public void onBackPressed() {
		if (appFinished)
			return;

		//Toast.makeText(this, "BACK button pressed.", Toast.LENGTH_SHORT).show();
	}

	public boolean onKey(View view, int keyCode, KeyEvent event) {
		if (appFinished) {
			if (DEBUG)
				Log.d(TAG, "Got KeyEvent after appFinished: " + event.toString());
			return false;
		}

		if (keyCode == KeyEvent.KEYCODE_ENTER) {
			event.dispatch(this, null, null);
			return true;
		}
		return false;
	}

    public void onItemSelected(AdapterView parent, View view, int position, long id) {
		if (appFinished)
			return;

		switch (showingMenu) {
			case MENU_MAIN:
				if (gameLoaded && (id == 3 || id == 4)) {
					menuSummary.setText(R.string.menu_savestate_help);
				} else {
					menuSummary.setText("");
				}
				break;
			case MENU_SETTINGS_EMU:
				if (id == 0)
					menuSummary.setText(R.string.pref_pcsx_rearmed_region_summ);
				else if (id == 1)
					menuSummary.setText(R.string.pref_pcsx_rearmed_drc_summ);
				else if (id == 2)
					menuSummary.setText(R.string.pref_pcsx_rearmed_bios_dir_summ);
				else if (id == 3)
					menuSummary.setText(R.string.pref_pcsx_rearmed_bios_file_summ);
				else if (id == 4)
					menuSummary.setText(R.string.pref_pcsx_rearmed_save_dir_summ);
				else if (id == 5)
					menuSummary.setText(R.string.pref_pcsx_rearmed_memcard1_summ);
				else if (id == 6)
					menuSummary.setText(R.string.pref_pcsx_rearmed_memcard2_summ);
				else
					menuSummary.setText("");
				break;
			case MENU_SETTINGS_VID:
				if (id == 0)
					menuSummary.setText(R.string.pref_pcsx_rearmed_frameskip_summ);
				else if (id == 1)
					menuSummary.setText(R.string.pref_pcsx_rearmed_neon_interlace_enable_summ);
				else if (id == 2)
					menuSummary.setText(R.string.pref_pcsx_rearmed_neon_scaling_summ);
				else if (id == 3)
					menuSummary.setText(R.string.pref_pcsx_rearmed_scanlines_summ);
				else if (id == 4)
					menuSummary.setText(R.string.pref_pcsx_rearmed_duping_enable_summ);
				else if (id == 5)
					menuSummary.setText(R.string.pref_maintain_psx_aspect_ratio_summ);
				else
					menuSummary.setText("");
				break;
			case MENU_SETTINGS_AUD:
				if (id == 0)
					menuSummary.setText(R.string.pref_pcsx_rearmed_spu_reverb_summ);
				else if (id == 1)
					menuSummary.setText(R.string.pref_pcsx_rearmed_spu_interpolation_summ);
				else if (id == 2)
					menuSummary.setText(R.string.pref_pcsx_rearmed_spu_irq_always_on_summ);
				else if (id == 3)
					menuSummary.setText(R.string.pref_pcsx_rearmed_disable_xa_summ);
				else if (id == 4)
					menuSummary.setText(R.string.pref_pcsx_rearmed_disable_cdda_summ);
				else
					menuSummary.setText("");
				break;
			case MENU_SETTINGS_INP:
				if (id == 0)
					menuSummary.setText(R.string.pref_pcsx_rearmed_pad1type_summ);
				else if (id == 1)
					menuSummary.setText(R.string.pref_emulate_select_start_touchpad_summ);
				else if (id == 2)
					menuSummary.setText(R.string.pref_pcsx_rearmed_pad2type_summ);
				else if (id == 3)
					menuSummary.setText(R.string.pref_emulate_select_start_touchpad_summ);
				else if (id == 4)
					menuSummary.setText(R.string.menu_item_keybinds);
				else
					menuSummary.setText("");
				break;
			case MENU_SETTINGS_BIOS:
			case MENU_SETTINGS_SAVE:
			case MENU_SETTINGS_KEYBINDS:
			case MENU_LOAD_GAME:
			case MENU_CHANGE_DISC:
				break;
			default:
				menuSummary.setText("");
				break;
		}		
	}
	
    public void onNothingSelected(AdapterView parent) {
		if (appFinished)
			return;

		menuSummary.setText("");
	}
	
    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        nativeSetSurface(holder.getSurface());
    }

    public void surfaceCreated(SurfaceHolder holder) {
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        nativeSetSurface(null);
    }
    
	public void onClick(DialogInterface dialogIntf, int which) {
		if (appFinished)
			return;

		dialogIntf.dismiss();
		int id = (int)menuList.getSelectedItemId();
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		SharedPreferences.Editor prefEdit = prefs.edit();
		String[] values;
		String val;
		switch (showingMenu) {
			case MENU_SETTINGS_EMU:
				switch (id) {
					case 0:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_region_values);
						prefEdit.putString("pref_pcsx_rearmed_region", values[which]);
						break;
					case 1:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_enabled_disabled_values);
						prefEdit.putString("pref_pcsx_rearmed_drc", values[which]);
						break;
					case 3:
						val = (String)((AlertDialog)dialogIntf).getListView().getAdapter().getItem(which);
						prefEdit.putString("pref_pcsx_rearmed_bios_file", val);
						break;
					case 5:
						val = (String)((AlertDialog)dialogIntf).getListView().getAdapter().getItem(which);
						prefEdit.putString("pref_pcsx_rearmed_memcard1", val);
						break;
					case 6:
						val = (String)((AlertDialog)dialogIntf).getListView().getAdapter().getItem(which);
						prefEdit.putString("pref_pcsx_rearmed_memcard2", val);
						break;
					default:
						return;
				}
				break;
			case MENU_SETTINGS_VID:
				switch (id) {
					case 0:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_frameskip_values);
						prefEdit.putString("pref_pcsx_rearmed_frameskip", values[which]);
						break;
					case 1:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_enabled_disabled_values);
						prefEdit.putString("pref_pcsx_rearmed_neon_interlace_enable", values[which]);
						break;
					case 2:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_neon_scaling_values);
						prefEdit.putString("pref_pcsx_rearmed_neon_scaling", values[which]);
						break;
					case 3:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_scanlines_values);
						prefEdit.putString("pref_pcsx_rearmed_scanlines", values[which]);
						break;
					case 4:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_values);
						prefEdit.putString("pref_pcsx_rearmed_duping_enable", values[which]);
						break;
					case 5:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_values);
						prefEdit.putString("pref_maintain_psx_aspect_ratio", values[which]);
						setSurfaceAspectRatio(values[which].equals("on"));
						break;
					default:
						return;
				}
				break;
			case MENU_SETTINGS_AUD:
				switch (id) {
					case 0:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_values);
						prefEdit.putString("pref_pcsx_rearmed_spu_reverb", values[which]);
						break;
					case 1:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_spu_interpolation_values);
						prefEdit.putString("pref_pcsx_rearmed_spu_interpolation", values[which]);
						break;
					case 2:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_values);
						prefEdit.putString("pref_pcsx_rearmed_spu_irq_always_on", values[which]);
						break;
					case 3:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_values);
						prefEdit.putString("pref_pcsx_rearmed_disable_xa", values[which]);
						break;
					case 4:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_values);
						prefEdit.putString("pref_pcsx_rearmed_disable_cdda", values[which]);
						break;
					default:
						return;
				}
				break;
			case MENU_SETTINGS_INP:
				switch (id) {
					case 0:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_padtype_values);
						prefEdit.putString("pref_pcsx_rearmed_pad1type", values[which]);
						break;
					case 1:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_enabled_disabled_values);
						prefEdit.putString("pref_emulate_select_start_touchpad_pad1", values[which]);
						break;
					case 2:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_padtype_values);
						prefEdit.putString("pref_pcsx_rearmed_pad2type", values[which]);
						break;
					case 3:
						values = getResources().getStringArray(R.array.pref_pcsx_rearmed_enabled_disabled_values);
						prefEdit.putString("pref_emulate_select_start_touchpad_pad2", values[which]);
						break;
					default:
						return;
				}
				break;
			default:
				return;
		}
		prefEdit.apply();
		setPadIsAnalog();
		setEmuSelectStart();
		nativePrefsChanged();
		displayMenu(showingMenu, id);
	}
	
	public void onDismiss(DialogInterface dialog) {
		if (appFinished)
			return;

		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		SharedPreferences.Editor prefEdit = prefs.edit();
		prefEdit.putInt("pref_keybind_p" + kbd.player + "_key" + kbd.keybindTarget, kbd.inputCode);
		prefEdit.apply();
		keyBinds[kbd.player][kbd.keybindTarget] = kbd.inputCode;
		
		int selection = (kbd.player*20)+kbd.keybindTarget;
		View c = menuList.getSelectedView();
		int top = (c == null) ? 0 : (c.getTop() - menuList.getPaddingTop());
		displayMenu(MENU_SETTINGS_KEYBINDS);
		menuList.setSelectionFromTop(selection, top);
		
		kbd = null;
	}

	public static int getWidestView(Context context, Adapter adapter, double mult) {
		int maxWidth = 0;
		View view = null;
		FrameLayout fakeParent = new FrameLayout(context);
		for (int i=0, count=adapter.getCount(); i<count; i++) {
			view = adapter.getView(i, view, fakeParent);
			view.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
			int width = view.getMeasuredWidth();
			if (width > maxWidth) {
				maxWidth = width;
			}
		}
		return (int)(maxWidth * mult);
	}

	public static int getWidestView(Context context, Adapter adapter) {
		return getWidestView(context, adapter, 1.0);
	}
	
    public void displayMenu(int type) {
		File dir;
		String[] filelist;
		switch (type) {
			case MENU_NONE:
				menuTitle.setText("");
				menuSubtitle.setText("");
				menuSummary.setText("");
				menuList.setAdapter(null);
				menuScreen.setVisibility(View.GONE);
				showingMenu = MENU_NONE;
				break;
			case MENU_MAIN:
				if (DEBUG)
					Log.d(TAG, "Displaying main menu");
				menuTitle.setText(R.string.menu_main);
				menuSubtitle.setText("");
				if (gameLoaded)
					menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, menuMainLoadedArray));
				else
					menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, menuMainNoloadArray));
				menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
				menuScreen.setVisibility(View.VISIBLE);
				showingMenu = MENU_MAIN;
				break;
			case MENU_SETTINGS:
				menuTitle.setText(R.string.menu_item_settings);
				menuSubtitle.setText("");
				menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, menuSettingsArray));
				menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
				menuScreen.setVisibility(View.VISIBLE);
				showingMenu = MENU_SETTINGS;
				break;
			case MENU_SETTINGS_EMU:
				menuTitle.setText(R.string.prefcat_emu);
				menuSubtitle.setText("");
				menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, getPrefsArray(MENU_SETTINGS_EMU)));
				menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
				menuScreen.setVisibility(View.VISIBLE);
				showingMenu = MENU_SETTINGS_EMU;
				break;
			case MENU_SETTINGS_VID:
				menuTitle.setText(R.string.prefcat_vid);
				menuSubtitle.setText("");
				menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, getPrefsArray(MENU_SETTINGS_VID)));
				menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
				menuScreen.setVisibility(View.VISIBLE);
				showingMenu = MENU_SETTINGS_VID;
				break;
			case MENU_SETTINGS_AUD:
				menuTitle.setText(R.string.prefcat_aud);
				menuSubtitle.setText("");
				menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, getPrefsArray(MENU_SETTINGS_AUD)));
				menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
				menuScreen.setVisibility(View.VISIBLE);
				showingMenu = MENU_SETTINGS_AUD;
				break;
			case MENU_SETTINGS_INP:
				menuTitle.setText(R.string.prefcat_inp);
				menuSubtitle.setText("");
				menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, getPrefsArray(MENU_SETTINGS_INP)));
				menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
				menuScreen.setVisibility(View.VISIBLE);
				showingMenu = MENU_SETTINGS_INP;
				break;
			case MENU_SETTINGS_BIOS:
				menuTitle.setText(R.string.pref_pcsx_rearmed_bios_dir_browse);
				browseBIOSPath = PreferenceManager.getDefaultSharedPreferences(this).getString("pref_pcsx_rearmed_bios_dir", "");
				dir = new File(browseBIOSPath);
				if (!dir.exists() || !dir.isDirectory()) {
					browseBIOSPath = getString(R.string.pref_pcsx_rearmed_path_default);
					dir = new File(browseBIOSPath);
				}
				filelist = dir.list();
				Arrays.sort(filelist);
				menuSubtitle.setText(browseBIOSPath);
				menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, filelist));
				menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
				menuSummary.setText(R.string.menu_browse_dir_help);
				menuScreen.setVisibility(View.VISIBLE);
				showingMenu = MENU_SETTINGS_BIOS;
				break;
			case MENU_SETTINGS_SAVE:
				menuTitle.setText(R.string.pref_pcsx_rearmed_save_dir_browse);
				browseSavePath = PreferenceManager.getDefaultSharedPreferences(this).getString("pref_pcsx_rearmed_save_dir", "");
				dir = new File(browseSavePath);
				if (!dir.exists() || !dir.isDirectory()) {
					browseSavePath = getString(R.string.pref_pcsx_rearmed_path_default);
					dir = new File(browseSavePath);
				}
				filelist = dir.list();
				Arrays.sort(filelist);
				menuSubtitle.setText(browseSavePath);
				menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, filelist));
				menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
				menuSummary.setText(R.string.menu_browse_dir_help);
				menuScreen.setVisibility(View.VISIBLE);
				showingMenu = MENU_SETTINGS_SAVE;
				break;
			case MENU_SETTINGS_KEYBINDS:
				menuTitle.setText(R.string.menu_item_keybinds);
				menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, getPrefsArray(MENU_SETTINGS_KEYBINDS)));
				menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
				menuSummary.setText(R.string.menu_keybind_help);
				menuScreen.setVisibility(View.VISIBLE);
				showingMenu = MENU_SETTINGS_KEYBINDS;
				break;
			case MENU_LOAD_GAME:
				menuTitle.setText(R.string.menu_item_loadgame);
				browseROMPath = PreferenceManager.getDefaultSharedPreferences(this).getString("pref_pcsx_rearmed_rom_dir", "");
				dir = new File(browseROMPath);
				if (!dir.exists() || !dir.isDirectory()) {
					browseROMPath = getString(R.string.pref_pcsx_rearmed_path_default);
					dir = new File(browseROMPath);
				}
				filelist = dir.list();
				Arrays.sort(filelist);
				menuSubtitle.setText(browseROMPath);
				menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, filelist));
				menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
				menuSummary.setText(R.string.menu_browse_file_help);
				menuScreen.setVisibility(View.VISIBLE);
				showingMenu = MENU_LOAD_GAME;
				break;
			case MENU_CHANGE_DISC:
				menuTitle.setText(R.string.menu_item_changedisc);
				browseROMPath = PreferenceManager.getDefaultSharedPreferences(this).getString("pref_pcsx_rearmed_rom_dir", "");
				dir = new File(browseROMPath);
				if (!dir.exists() || !dir.isDirectory()) {
					browseROMPath = getString(R.string.pref_pcsx_rearmed_path_default);
					dir = new File(browseROMPath);
				}
				filelist = dir.list();
				Arrays.sort(filelist);
				menuSubtitle.setText(browseROMPath);
				menuList.setAdapter(new MenuAdapter<String>(this, R.layout.menuitem, filelist));
				menuList.getLayoutParams().width = getWidestView(this, menuList.getAdapter(), 1.05);
				menuSummary.setText(R.string.menu_browse_file_help);
				menuScreen.setVisibility(View.VISIBLE);
				showingMenu = MENU_CHANGE_DISC;
				break;
			default:
				break;
		}
		if (type != MENU_NONE) {
			menuList.requestFocus();
		}
	}

    public void displayMenu(int type, int selection) {
		displayMenu(type);
		menuList.setSelection(selection);
		if (type != MENU_NONE) {
			menuList.requestFocus();
		}
	}
	
    public void setSurfaceAspectRatio(boolean which) {

		RelativeLayout.LayoutParams lp = new RelativeLayout.LayoutParams(surfacev.getLayoutParams());
		
		if (which) {
			Display display = getWindowManager().getDefaultDisplay();
			Point size = new Point();
			display.getSize(size);

			if (DEBUG)
				Log.d(TAG, "Screen size is: " + size.x + " x " + size.y);

			lp.width = (int)(size.y * (4.0 / 3));
			lp.height = size.y;
			lp.alignWithParent = true;
			lp.addRule(RelativeLayout.CENTER_IN_PARENT);
			
			if (DEBUG)
				Log.d(TAG, "SurfaceView LP set to size: " + lp.width + " x " + lp.height);

			surfacev.setLayoutParams(lp);
		} else {
			//~ AttributeSet as = Xml.asAttributeSet(getResources().getXml(R.id.surfaceview));
			//~ surfacev.setLayoutParams(new RelativeLayout.LayoutParams(this, as));

			lp.height = RelativeLayout.LayoutParams.MATCH_PARENT;
			lp.width = RelativeLayout.LayoutParams.MATCH_PARENT;

			if (DEBUG)
				Log.d(TAG, "SurfaceView LP set to size: " + lp.width + " x " + lp.height);

			surfacev.setLayoutParams(lp);
		}
		surfacev.requestLayout();
	}

	public void showKeybindDialog(long id) {
		int player = (int) id / 20;
		int target = (int) id - (player * 20);
		
		if (OuyaController.getControllerByPlayer(player) == null) {
			String err = getString(R.string.string_player) + " " + (player+1) + " " + getString(R.string.error_nocontroller);
			Toast.makeText(this, err, Toast.LENGTH_SHORT).show();
			return;
		}
		
		kbd = new KeybindDialog(this);
		
		TextView titleView = new TextView(this);
		titleView.setText(getString(R.string.string_player) + " " + (player+1) + " " + getKeybindName(target));
		titleView.setTypeface(gameFont);
		titleView.setTextColor(Color.WHITE);
		titleView.setBackgroundColor(Color.BLACK);
		titleView.setPadding(100, 10, 100, 10);
		titleView.setGravity(Gravity.CENTER);
		kbd.setCustomTitle(titleView);
		TextView contentView = new TextView(this);
		contentView.setText((target < 16) ? R.string.string_bind_button : R.string.string_bind_axis);
		contentView.setTypeface(gameFont);
		contentView.setTextColor(Color.WHITE);
		contentView.setBackgroundColor(Color.BLACK);
		contentView.setPadding(100, 100, 100, 100);
		contentView.setGravity(Gravity.CENTER);
		kbd.setView(contentView);
		kbd.setOnDismissListener(this);
		kbd.player = player;
		kbd.gettingKey = (target < 16);
		kbd.keybindTarget = target;
		kbd.show();
	}

	public void showBIOSOptions() {
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		int titleInt;
		String prefStr, val;
		String[] items;
		File dir;
		prefStr = "pref_pcsx_rearmed_bios_file";
		titleInt = R.string.pref_pcsx_rearmed_bios_file;

		FilenameFilter binFilter = new FilenameFilter() {
			public boolean accept(File dir, String name) {
				if (name.toLowerCase().endsWith(".bin")) {
					return true;
				} else {
					return false;
				}
			}
		};

		browseBIOSPath = prefs.getString("pref_pcsx_rearmed_bios_dir", "");
		dir = new File(browseBIOSPath);
		if (!dir.exists() || !dir.isDirectory()) {
			Toast.makeText(this, R.string.menu_browse_noacc, Toast.LENGTH_SHORT).show();
			return;
		}
		items = dir.list(binFilter);
		if (items.length == 0) {
			Toast.makeText(this, R.string.menu_bios_nofiles, Toast.LENGTH_SHORT).show();
			return;
		}
		Arrays.sort(items);
		val = prefs.getString(prefStr, "");
		int pos = Arrays.asList(items).indexOf(val);

		TextView titleView = new TextView(this);
		titleView.setText(titleInt);
		titleView.setTypeface(gameFont);
		titleView.setBackgroundColor(Color.BLACK);
		titleView.setPadding(100, 10, 100, 10);
		titleView.setGravity(Gravity.CENTER);
		builder.setCustomTitle(titleView);
		builder.setSingleChoiceItems(new MenuAdapter<String>(this, R.layout.dialogitem, items), pos, this);

		AlertDialog dialog = builder.show();

		ListView lv = dialog.getListView();
		lv.setBackgroundColor(Color.TRANSPARENT);
		((View)lv.getParent()).setBackgroundResource(R.drawable.popup_bottom_dark);
		((LinearLayout.LayoutParams)lv.getLayoutParams()).gravity = Gravity.CENTER;
		((LinearLayout.LayoutParams)lv.getLayoutParams()).bottomMargin = 20;
		lv.getLayoutParams().width = getWidestView(getApplicationContext(), lv.getAdapter(), 1.05);
	}

	public void showMemcardOptions(int slot) {
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		int titleInt;
		String prefStr, val;
		String[] items;
		File dir;
		if (slot == 1) {
			prefStr = "pref_pcsx_rearmed_memcard1";
			titleInt = R.string.pref_pcsx_rearmed_memcard1;
		} else if (slot == 2) {
			prefStr = "pref_pcsx_rearmed_memcard2";
			titleInt = R.string.pref_pcsx_rearmed_memcard2;
		} else {
			return;
		}

		FilenameFilter mcdFilter = new FilenameFilter() {
			public boolean accept(File dir, String name) {
				if (name.toLowerCase().endsWith(".mcd")) {
					return true;
				} else {
					return false;
				}
			}
		};

		browseSavePath = prefs.getString("pref_pcsx_rearmed_save_dir", "");
		dir = new File(browseSavePath);
		if (!dir.exists() || !dir.isDirectory()) {
			Toast.makeText(this, R.string.menu_browse_noacc, Toast.LENGTH_SHORT).show();
			return;
		}
		items = dir.list(mcdFilter);
		if (items.length == 0) {
			Toast.makeText(this, R.string.menu_memcard_nofiles, Toast.LENGTH_SHORT).show();
			return;
		}
		Arrays.sort(items);
		val = prefs.getString(prefStr, "");
		int pos = Arrays.asList(items).indexOf(val);

		TextView titleView = new TextView(this);
		titleView.setText(titleInt);
		titleView.setTypeface(gameFont);
		titleView.setBackgroundColor(Color.BLACK);
		titleView.setPadding(100, 10, 100, 10);
		titleView.setGravity(Gravity.CENTER);
		builder.setCustomTitle(titleView);
		builder.setSingleChoiceItems(new MenuAdapter<String>(this, R.layout.dialogitem, items), pos, this);

		AlertDialog dialog = builder.show();

		ListView lv = dialog.getListView();
		lv.setBackgroundColor(Color.TRANSPARENT);
		((View)lv.getParent()).setBackgroundResource(R.drawable.popup_bottom_dark);
		((LinearLayout.LayoutParams)lv.getLayoutParams()).gravity = Gravity.CENTER;
		((LinearLayout.LayoutParams)lv.getLayoutParams()).bottomMargin = 20;
		lv.getLayoutParams().width = getWidestView(getApplicationContext(), lv.getAdapter(), 1.05);
	}

	public void showSettingsForPref(String prefStr, int titleInt, int itemsInt, int valuesInt) {
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		String val = prefs.getString(prefStr, "");
		String[] items = getResources().getStringArray(itemsInt);
		String[] values = getResources().getStringArray(valuesInt);
		int pos = Arrays.asList(values).indexOf(val);
		
		TextView titleView = new TextView(this);
		titleView.setText(titleInt);
		titleView.setTypeface(gameFont);
		titleView.setBackgroundColor(Color.BLACK);
		titleView.setPadding(100, 10, 100, 10);
		titleView.setGravity(Gravity.CENTER);
		builder.setCustomTitle(titleView);
		builder.setSingleChoiceItems(new MenuAdapter<String>(this, R.layout.dialogitem, items), pos, this);

		AlertDialog dialog = builder.show();
		
		ListView lv = dialog.getListView();
		lv.setBackgroundColor(Color.TRANSPARENT);
		((View)lv.getParent()).setBackgroundResource(R.drawable.popup_bottom_dark);
		((LinearLayout.LayoutParams)lv.getLayoutParams()).gravity = Gravity.CENTER;
		((LinearLayout.LayoutParams)lv.getLayoutParams()).bottomMargin = 20;
		lv.getLayoutParams().width = getWidestView(getApplicationContext(), lv.getAdapter(), 1.05);
	}
	
	public void showSettingsForMenu() {
		int id = (int)menuList.getSelectedItemId();
		switch (showingMenu) {
			case MENU_SETTINGS_EMU:
				switch (id) {
					case 0:
						showSettingsForPref("pref_pcsx_rearmed_region", 
											R.string.pref_pcsx_rearmed_region, 
											R.array.pref_pcsx_rearmed_region_entries, 
											R.array.pref_pcsx_rearmed_region_values);
						return;
					case 1:
						showSettingsForPref("pref_pcsx_rearmed_drc", 
											R.string.pref_pcsx_rearmed_drc, 
											R.array.pref_pcsx_rearmed_enabled_disabled_entries, 
											R.array.pref_pcsx_rearmed_enabled_disabled_values);
						return;
					case 2:
						displayMenu(MENU_SETTINGS_BIOS);
						return;
					case 3:
						showBIOSOptions();
						return;
					case 4:
						displayMenu(MENU_SETTINGS_SAVE);
						return;
					case 5:
						showMemcardOptions(1);
						return;
					case 6:
						showMemcardOptions(2);
						return;
					default:
						return;
				}
			case MENU_SETTINGS_VID:
				switch (id) {
					case 0:
						showSettingsForPref("pref_pcsx_rearmed_frameskip", 
											R.string.pref_pcsx_rearmed_frameskip, 
											R.array.pref_pcsx_rearmed_frameskip_entries, 
											R.array.pref_pcsx_rearmed_frameskip_values);
						return;
					case 1:
						showSettingsForPref("pref_pcsx_rearmed_neon_interlace_enable", 
											R.string.pref_pcsx_rearmed_neon_interlace_enable, 
											R.array.pref_pcsx_rearmed_enabled_disabled_entries, 
											R.array.pref_pcsx_rearmed_enabled_disabled_values);
						return;
					case 2:
						showSettingsForPref("pref_pcsx_rearmed_neon_scaling", 
											R.string.pref_pcsx_rearmed_neon_scaling, 
											R.array.pref_pcsx_rearmed_neon_scaling_entries, 
											R.array.pref_pcsx_rearmed_neon_scaling_values);
						return;
					case 3:
						showSettingsForPref("pref_pcsx_rearmed_scanlines", 
											R.string.pref_pcsx_rearmed_scanlines, 
											R.array.pref_pcsx_rearmed_scanlines_entries, 
											R.array.pref_pcsx_rearmed_scanlines_values);
						return;
					case 4:
						showSettingsForPref("pref_pcsx_rearmed_duping_enable", 
											R.string.pref_pcsx_rearmed_duping_enable, 
											R.array.pref_pcsx_rearmed_on_off_entries, 
											R.array.pref_pcsx_rearmed_on_off_values);
						return;
					case 5:
						showSettingsForPref("pref_maintain_psx_aspect_ratio", 
											R.string.pref_maintain_psx_aspect_ratio, 
											R.array.pref_pcsx_rearmed_on_off_entries, 
											R.array.pref_pcsx_rearmed_on_off_values);
						return;
					default:
						return;
				}
			case MENU_SETTINGS_AUD:
				switch (id) {
					case 0:
						showSettingsForPref("pref_pcsx_rearmed_spu_reverb", 
											R.string.pref_pcsx_rearmed_spu_reverb, 
											R.array.pref_pcsx_rearmed_on_off_entries, 
											R.array.pref_pcsx_rearmed_on_off_values);
						return;
					case 1:
						showSettingsForPref("pref_pcsx_rearmed_spu_interpolation", 
											R.string.pref_pcsx_rearmed_spu_interpolation, 
											R.array.pref_pcsx_rearmed_spu_interpolation_entries, 
											R.array.pref_pcsx_rearmed_spu_interpolation_values);
						return;
					case 2:
						showSettingsForPref("pref_pcsx_rearmed_spu_irq_always_on", 
											R.string.pref_pcsx_rearmed_spu_irq_always_on, 
											R.array.pref_pcsx_rearmed_on_off_entries, 
											R.array.pref_pcsx_rearmed_on_off_values);
						return;
					case 3:
						showSettingsForPref("pref_pcsx_rearmed_disable_xa", 
											R.string.pref_pcsx_rearmed_disable_xa, 
											R.array.pref_pcsx_rearmed_on_off_entries, 
											R.array.pref_pcsx_rearmed_on_off_values);
						return;
					case 4:
						showSettingsForPref("pref_pcsx_rearmed_disable_cdda", 
											R.string.pref_pcsx_rearmed_disable_cdda, 
											R.array.pref_pcsx_rearmed_on_off_entries, 
											R.array.pref_pcsx_rearmed_on_off_values);
						return;
					default:
						return;
				}
			case MENU_SETTINGS_INP:
				switch (id) {
					case 0:
						showSettingsForPref("pref_pcsx_rearmed_pad1type", 
											R.string.pref_pcsx_rearmed_pad1type, 
											R.array.pref_pcsx_rearmed_padtype_entries, 
											R.array.pref_pcsx_rearmed_padtype_values);
						return;
					case 1:
						showSettingsForPref("pref_emulate_select_start_touchpad_pad1", 
											R.string.pref_emulate_select_start_touchpad_pad1, 
											R.array.pref_pcsx_rearmed_enabled_disabled_entries, 
											R.array.pref_pcsx_rearmed_enabled_disabled_values);
						return;
					case 2:
						showSettingsForPref("pref_pcsx_rearmed_pad2type", 
											R.string.pref_pcsx_rearmed_pad2type, 
											R.array.pref_pcsx_rearmed_padtype_entries, 
											R.array.pref_pcsx_rearmed_padtype_values);
						return;
					case 3:
						showSettingsForPref("pref_emulate_select_start_touchpad_pad2", 
											R.string.pref_emulate_select_start_touchpad_pad2, 
											R.array.pref_pcsx_rearmed_enabled_disabled_entries, 
											R.array.pref_pcsx_rearmed_enabled_disabled_values);
						return;
					case 4:
						displayMenu(MENU_SETTINGS_KEYBINDS);
						return;
					default:
						return;
				}
		}
	}

	public String[] getPrefsArray(int which) {
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		String[] prefStrings, items, values;
		String item;
		int i, j;
		switch (which) {
			case MENU_SETTINGS_EMU:
				prefStrings = new String[7];

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_region_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_region_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_pcsx_rearmed_region", ""))];
				prefStrings[0] = getString(R.string.pref_pcsx_rearmed_region) + ": " + item;

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_enabled_disabled_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_enabled_disabled_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_pcsx_rearmed_drc", ""))];
				prefStrings[1] = getString(R.string.pref_pcsx_rearmed_drc) + ": " + item;

				prefStrings[2] = getString(R.string.pref_pcsx_rearmed_bios_dir);

				item = prefs.getString("pref_pcsx_rearmed_bios_file", "");
				prefStrings[3] = getString(R.string.pref_pcsx_rearmed_bios_file) + ": " + item;

				prefStrings[4] = getString(R.string.pref_pcsx_rearmed_save_dir);

				item = prefs.getString("pref_pcsx_rearmed_memcard1", "");
				prefStrings[5] = getString(R.string.pref_pcsx_rearmed_memcard1) + ": " + item;

				item = prefs.getString("pref_pcsx_rearmed_memcard2", "");
				prefStrings[6] = getString(R.string.pref_pcsx_rearmed_memcard2) + ": " + item;

				break;
			case MENU_SETTINGS_VID:
				prefStrings = new String[6];

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_frameskip_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_frameskip_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_pcsx_rearmed_frameskip", ""))];
				prefStrings[0] = getString(R.string.pref_pcsx_rearmed_frameskip) + ": " + item;

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_enabled_disabled_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_enabled_disabled_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_pcsx_rearmed_neon_interlace_enable", ""))];
				prefStrings[1] = getString(R.string.pref_pcsx_rearmed_neon_interlace_enable) + ": " + item;

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_neon_scaling_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_neon_scaling_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_pcsx_rearmed_neon_scaling", ""))];
				prefStrings[2] = getString(R.string.pref_pcsx_rearmed_neon_scaling) + ": " + item;

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_scanlines_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_scanlines_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_pcsx_rearmed_scanlines", ""))];
				prefStrings[3] = getString(R.string.pref_pcsx_rearmed_scanlines) + ": " + item;

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_pcsx_rearmed_duping_enable", ""))];
				prefStrings[4] = getString(R.string.pref_pcsx_rearmed_duping_enable) + ": " + item;

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_maintain_psx_aspect_ratio", ""))];
				prefStrings[5] = getString(R.string.pref_maintain_psx_aspect_ratio) + ": " + item;

				break;
			case MENU_SETTINGS_AUD:
				prefStrings = new String[5];

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_pcsx_rearmed_spu_reverb", ""))];
				prefStrings[0] = getString(R.string.pref_pcsx_rearmed_spu_reverb) + ": " + item;

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_spu_interpolation_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_spu_interpolation_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_pcsx_rearmed_spu_interpolation", ""))];
				prefStrings[1] = getString(R.string.pref_pcsx_rearmed_spu_interpolation) + ": " + item;

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_pcsx_rearmed_spu_irq_always_on", ""))];
				prefStrings[2] = getString(R.string.pref_pcsx_rearmed_spu_irq_always_on) + ": " + item;

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_pcsx_rearmed_disable_xa", ""))];
				prefStrings[3] = getString(R.string.pref_pcsx_rearmed_disable_xa) + ": " + item;

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_on_off_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_pcsx_rearmed_disable_cdda", ""))];
				prefStrings[4] = getString(R.string.pref_pcsx_rearmed_disable_cdda) + ": " + item;

				break;
			case MENU_SETTINGS_INP:
				prefStrings = new String[5];

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_padtype_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_padtype_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_pcsx_rearmed_pad1type", ""))];
				prefStrings[0] = getString(R.string.pref_pcsx_rearmed_pad1type) + ": " + item;

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_enabled_disabled_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_enabled_disabled_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_emulate_select_start_touchpad_pad1", ""))];
				prefStrings[1] = getString(R.string.pref_emulate_select_start_touchpad_pad1) + ": " + item;

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_padtype_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_padtype_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_pcsx_rearmed_pad2type", ""))];
				prefStrings[2] = getString(R.string.pref_pcsx_rearmed_pad2type) + ": " + item;

				items = getResources().getStringArray(R.array.pref_pcsx_rearmed_enabled_disabled_entries);
				values = getResources().getStringArray(R.array.pref_pcsx_rearmed_enabled_disabled_values);
				item = items[Arrays.asList(values).indexOf(prefs.getString("pref_emulate_select_start_touchpad_pad2", ""))];
				prefStrings[3] = getString(R.string.pref_emulate_select_start_touchpad_pad2) + ": " + item;

				prefStrings[4] = getString(R.string.menu_item_keybinds);

				break;
			case MENU_SETTINGS_KEYBINDS:
				prefStrings = new String[40];
				for (i=0; i<2; i++) {
					for (j=0; j<20; j++) {
						item = getString(R.string.string_player) + " " + (i+1) + " " + getKeybindName(j) + ": ";
						if (j < 16)
							item += KeyEvent.keyCodeToString(keyBinds[i][j]).replace("KEYCODE_", "").replace("_", " ");
						else
							item += MotionEvent.axisToString(keyBinds[i][j]).replace("_", " ");
						prefStrings[(i*20)+j] = item;
					}
				}
				break;
			default:
				prefStrings = new String[0];
		}
		return prefStrings;
	}

	public String getKeybindName(int which) {
		switch (which) {
			case OUYA_PAD_O:
				return getString(R.string.string_psx_cross);
			case OUYA_PAD_U:
				return getString(R.string.string_psx_square);
			case OUYA_PAD_Y:
				return getString(R.string.string_psx_triangle);
			case OUYA_PAD_A:
				return getString(R.string.string_psx_circle);
			case OUYA_PAD_UP:
				return getString(R.string.string_psx_up);
			case OUYA_PAD_DOWN:
				return getString(R.string.string_psx_down);
			case OUYA_PAD_LEFT:
				return getString(R.string.string_psx_left);
			case OUYA_PAD_RIGHT:
				return getString(R.string.string_psx_right);
			case OUYA_PAD_L1:
				return getString(R.string.string_psx_l1);
			case OUYA_PAD_R1:
				return getString(R.string.string_psx_r1);
			case OUYA_PAD_L2:
				return getString(R.string.string_psx_l2);
			case OUYA_PAD_R2:
				return getString(R.string.string_psx_r2);
			case OUYA_PAD_L3:
				return getString(R.string.string_psx_l3);
			case OUYA_PAD_R3:
				return getString(R.string.string_psx_r3);
			case OUYA_PAD_SELECT:
				return getString(R.string.string_psx_select);
			case OUYA_PAD_START:
				return getString(R.string.string_psx_start);
			case OUYA_AXIS_LX:
				return getString(R.string.string_psx_lx);
			case OUYA_AXIS_LY:
				return getString(R.string.string_psx_ly);
			case OUYA_AXIS_RX:
				return getString(R.string.string_psx_rx);
			case OUYA_AXIS_RY:
				return getString(R.string.string_psx_ry);
		}
		return null;
	}

	public int getDefaultKeybindKey(int keyBind) {
		switch (keyBind) {
			case OUYA_PAD_O:
				return OuyaController.BUTTON_O;
			case OUYA_PAD_U:
				return OuyaController.BUTTON_U;
			case OUYA_PAD_Y:
				return OuyaController.BUTTON_Y;
			case OUYA_PAD_A:
				return OuyaController.BUTTON_A;
			case OUYA_PAD_UP:
				return OuyaController.BUTTON_DPAD_UP;
			case OUYA_PAD_DOWN:
				return OuyaController.BUTTON_DPAD_DOWN;
			case OUYA_PAD_LEFT:
				return OuyaController.BUTTON_DPAD_LEFT;
			case OUYA_PAD_RIGHT:
				return OuyaController.BUTTON_DPAD_RIGHT;
			case OUYA_PAD_L1:
				return OuyaController.BUTTON_L1;
			case OUYA_PAD_R1:
				return OuyaController.BUTTON_R1;
			case OUYA_PAD_L2:
				return KeyEvent.KEYCODE_BUTTON_L2;
			case OUYA_PAD_R2:
				return KeyEvent.KEYCODE_BUTTON_R2;
			case OUYA_PAD_L3:
				return OuyaController.BUTTON_L3;
			case OUYA_PAD_R3:
				return OuyaController.BUTTON_R3;
			case OUYA_PAD_SELECT:
				return KeyEvent.KEYCODE_BUTTON_SELECT;
			case OUYA_PAD_START:
				return KeyEvent.KEYCODE_BUTTON_START;
			case OUYA_AXIS_LX:
				return OuyaController.AXIS_LS_X;
			case OUYA_AXIS_LY:
				return OuyaController.AXIS_LS_Y;
			case OUYA_AXIS_RX:
				return OuyaController.AXIS_RS_X;
			case OUYA_AXIS_RY:
				return OuyaController.AXIS_RS_Y;
		}
		return 0;
	}

	public void setDefaultKeybinds() {
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		SharedPreferences.Editor prefEdit = prefs.edit();
		int bind, i, j;
		
		if (keyBinds == null)
			keyBinds = new int[SUPPORTED_PLAYERS][20];
		
		for (i = 0; i < SUPPORTED_PLAYERS; i++) {
			for (j = 0; j < 20; j++) {
				bind = prefs.getInt("pref_keybind_p" + i + "_key" + j, 0);
				if (bind == 0) {
					bind = getDefaultKeybindKey(j);
					prefEdit.putInt("pref_keybind_p" + i + "_key" + j, bind);
					prefEdit.apply();
					keyBinds[i][j] = bind;
				} else {
					keyBinds[i][j] = bind;
				}
			}
		}
	}

	public void resetKeybind(long id) {
		int player = (int) id / 20;
		int target = (int) id - (player * 20);
		
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		SharedPreferences.Editor prefEdit = prefs.edit();
		int bind = getDefaultKeybindKey(target);
		prefEdit.putInt("pref_keybind_p" + player + "_key" + target, bind);
		prefEdit.apply();
		keyBinds[player][target] = bind;
	}
	
	public void setKeypresses(KeyEvent event) {
		int player, keycode, i, j;
		
		if (keyPresses == null) {
			keyPresses = new boolean[SUPPORTED_PLAYERS][16];
			for (i = 0; i < SUPPORTED_PLAYERS; i++) {
				for (j = 0; j < 16; j++) {
					keyPresses[i][j] = false;
				}
			}
		}
		
		if (event == null)
			return;

		player = OuyaController.getPlayerNumByDeviceId(event.getDeviceId());
		if (player < 0 || player > 3) {
			if (DEBUG)
				Log.e(TAG, "setKeypresses() - Invalid player: " + player);
			Toast.makeText(this, R.string.error_invalid_playerid_controller, Toast.LENGTH_SHORT).show();
			return;
		}
		keycode = event.getKeyCode();
		for (j = 0; j < 16; j++) {
			if (keyBinds[player][j] == keycode) {
				keyPresses[player][j] = (event.getAction() == KeyEvent.ACTION_DOWN);
				return;
			}
		}
		
	}
	
	public void setEmuSelectStart() {
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		int i;

		if (emuSelectStart == null) {
			emuSelectStart = new boolean[SUPPORTED_PLAYERS][4];

			emuSelectStartViews = new View[SUPPORTED_PLAYERS][3];
			for (i = 0; i < SUPPORTED_PLAYERS; i++) {
				emuSelectStartViews[i][EMU_SELSTA_VWGROUP] = findViewById(android.R.id.content).findViewWithTag("selectstartview" + i);
				emuSelectStartViews[i][EMU_SELSTA_VWSELECT] = findViewById(android.R.id.content).findViewWithTag("selectview" + i);
				emuSelectStartViews[i][EMU_SELSTA_VWSTART] = findViewById(android.R.id.content).findViewWithTag("startview" + i);
			}
		}

		for (i = 0; i < SUPPORTED_PLAYERS; i++) {
			if (prefs.getString("pref_emulate_select_start_touchpad_pad" + (i+1), "").equals("enabled"))
				emuSelectStart[i][EMU_SELSTA_ENABLED] = true;
			else
				emuSelectStart[i][EMU_SELSTA_ENABLED] = false;
			
			emuSelectStart[i][EMU_SELSTA_STARTED] = false;
			emuSelectStart[i][EMU_SELSTA_SELDOWN] = false;
			emuSelectStart[i][EMU_SELSTA_STADOWN] = false;

			emuSelectStartViews[i][EMU_SELSTA_VWGROUP].setVisibility(View.INVISIBLE);
		}
	}
	
	public void setPadIsAnalog() {
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		int i;

		if (padIsAnalog == null) {
			padIsAnalog = new boolean[SUPPORTED_PLAYERS];
		}
		
		for (i = 0; i < SUPPORTED_PLAYERS; i++) {
			if (prefs.getString("pref_pcsx_rearmed_pad" + (i+1) + "type", "").equals("analog"))
				padIsAnalog[i] = true;
			else
				padIsAnalog[i] = false;
		}
	}

	public void handleEmuSelectStartMotion(MotionEvent event) {
		int player = OuyaController.getPlayerNumByDeviceId(event.getDeviceId());
		
		// check for odd OUYA error
		if (player < 0 || player > 3) {
			if (DEBUG)
				Log.e(TAG, "handleEmuSelectStartMotion() - Invalid player: " + player);
			Toast.makeText(this, R.string.error_invalid_playerid_controller, Toast.LENGTH_SHORT).show();
			return;
		}
		if (player >= SUPPORTED_PLAYERS)
			return;

		if (emuSelectStart[player][EMU_SELSTA_ENABLED]) {
			if (emuSelectStart[player][EMU_SELSTA_STARTED] && (event.getHistorySize() > 0)) {
				if (event.getHistoricalX(0) < event.getX()) {
					//~ if (DEBUG)
						//~ Log.d("MotionEvent", "Player " + (player+1) + " moved to the right");
					emuSelectStart[player][EMU_SELSTA_SELDOWN] = false;
					emuSelectStart[player][EMU_SELSTA_STADOWN] = true;
					((ImageView)emuSelectStartViews[player][EMU_SELSTA_VWSELECT]).setColorFilter(Color.DKGRAY);
					((ImageView)emuSelectStartViews[player][EMU_SELSTA_VWSTART]).setColorFilter(Color.WHITE);
				} else if (event.getHistoricalX(0) > event.getX()) {
					//~ if (DEBUG)
						//~ Log.d("MotionEvent", "Player " + (player+1) + " moved to the left");
					emuSelectStart[player][EMU_SELSTA_SELDOWN] = true;
					emuSelectStart[player][EMU_SELSTA_STADOWN] = false;
					((ImageView)emuSelectStartViews[player][EMU_SELSTA_VWSELECT]).setColorFilter(Color.WHITE);
					((ImageView)emuSelectStartViews[player][EMU_SELSTA_VWSTART]).setColorFilter(Color.DKGRAY);
				} else {
					// cursor stuck at the edge of screen
				}
			}
		}
	}
	
	public void handleEmuSelectStartTouch(MotionEvent event) {
		int player = OuyaController.getPlayerNumByDeviceId(event.getDeviceId());

		// check for odd OUYA error
		if (player < 0 || player > 3) {
			if (DEBUG)
				Log.e(TAG, "handleEmuSelectStartTouch() - Invalid player: " + player);
			Toast.makeText(this, R.string.error_invalid_playerid_controller, Toast.LENGTH_SHORT).show();
			return;
		}
		if (player >= SUPPORTED_PLAYERS)
			return;

		if (emuSelectStart[player][EMU_SELSTA_ENABLED]) {
			if (!emuSelectStart[player][EMU_SELSTA_STARTED]) {
				emuSelectStart[player][EMU_SELSTA_STARTED] = true;
				emuSelectStart[player][EMU_SELSTA_SELDOWN] = false;
				emuSelectStart[player][EMU_SELSTA_STADOWN] = false;
				//~ if (DEBUG)
					//~ Log.d("TouchEvent", "Player " + (player+1) + " started");
				((ImageView)emuSelectStartViews[player][EMU_SELSTA_VWSELECT]).setColorFilter(Color.DKGRAY);
				((ImageView)emuSelectStartViews[player][EMU_SELSTA_VWSTART]).setColorFilter(Color.DKGRAY);
				emuSelectStartViews[player][EMU_SELSTA_VWGROUP].setVisibility(View.VISIBLE);
			} else {
				emuSelectStart[player][EMU_SELSTA_STARTED] = false;
				emuSelectStart[player][EMU_SELSTA_SELDOWN] = false;
				emuSelectStart[player][EMU_SELSTA_STADOWN] = false;
				//~ if (DEBUG)
					//~ Log.d("TouchEvent", "Player " + (player+1) + " stopped");
				emuSelectStartViews[player][EMU_SELSTA_VWGROUP].setVisibility(View.INVISIBLE);
			}
		}
	}
	
	public void handleEmuMenu(KeyEvent event) {
		if (event == null)
			return;

		if (event.getKeyCode() != KeyEvent.KEYCODE_BUTTON_START)
			return;

		if (!event.isLongPress())
			return;

		//~ int player = OuyaController.getPlayerNumByDeviceId(event.getDeviceId());
		//~ if (keyPresses[player][OUYA_PAD_SELECT] && keyPresses[player][OUYA_PAD_START]) {
			// emulate menu button press with SELECT and START
			//~ long time = android.os.SystemClock.uptimeMillis();
			//~ KeyEvent kev = new KeyEvent(time, time, KeyEvent.ACTION_DOWN,
										//~ KeyEvent.KEYCODE_MENU, 0, 0, event.getDeviceId(), 0);
			//~ kev.dispatch(this, event., null);
			dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MENU));
			dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_MENU));
		//~ }
	}

	public static String getPref(String key) {
        //~ if (DEBUG)
			//~ Log.d(TAG, "getPref() called with key " + key);
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(actObj);
		if (prefs.contains(key)) {
			/*
			if (key.equals("pref_pcsx_rearmed_frameskip") || key.equals("pref_pcsx_rearmed_region") ||
					key.equals("pref_pcsx_rearmed_pad1type") || key.equals("pref_pcsx_rearmed_pad2type") ||
					key.equals("pref_pcsx_rearmed_spu_interpolation") || key.equals("pref_pcsx_rearmed_bios_dir")) {
				return prefs.getString(key, "");
			}
			else if (key.equals("pref_pcsx_rearmed_drc") || key.equals("pref_pcsx_rearmed_neon_interlace_enable") ||
					key.equals("pref_pcsx_rearmed_neon_enhancement_enable") || 
					key.equals("pref_pcsx_rearmed_neon_enhancement_no_main") ||
					key.equals("pref_emulate_select_start_touchpad_pad1") || 
					key.equals("pref_emulate_select_start_touchpad_pad2")) {
				return (prefs.getBoolean(key, false)) ? "enabled" : "disabled";
			}
			else if (key.equals("pref_pcsx_rearmed_duping_enable") || key.equals("pref_pcsx_rearmed_spu_reverb")) {
				return (prefs.getBoolean(key, false)) ? "on" : "off";
			}
			*/
			return prefs.getString(key, "");
		}
		return "";
	}
	
	public static void setPCSXMessage(String msg) {
		actObj.pcsxMsgText = msg;
		actObj.runOnUiThread(new Runnable() {
			public void run() {
				actObj.pcsxMsg.setText(actObj.pcsxMsgText);
			}
		});
	}

	public static void loadError(String path) {
		actObj.loadErrorPath = path;
		actObj.runOnUiThread(new Runnable() {
			public void run() {
				actObj.gameLoaded = false;
				actObj.exeLoaded = false;
				actObj.displayMenu(MENU_MAIN);
				Toast.makeText(actObj, actObj.getString(R.string.error_load) + actObj.loadErrorPath, Toast.LENGTH_SHORT).show();
			}
		});
	}

	public static int getInputStateShift(OuyaController c) {
		int state, player, i;
		state = 0;
		player = c.getPlayerNum();
		for (i=0;i<16;i++) {
			//~ if (c.getButton(actObj.keyBinds[player][i]))
			if (actObj.keyPresses[player][i])
				state |= (1 << i);
		}
		return state;
	}

	private static int[] input = {0,0,0,0,0,0,0,0,0,0};

	public static int[] getInputState() {
		int btnstate, i;
		OuyaController c;
		
		for (i = 0; i < SUPPORTED_PLAYERS; i++) {
			btnstate = 0;
			c = OuyaController.getControllerByPlayer(i);
			if (c != null) {
				btnstate = getInputStateShift(c);
				// emulated select and start
				if (actObj.emuSelectStart[i][EMU_SELSTA_ENABLED]) {
					if (actObj.emuSelectStart[i][EMU_SELSTA_SELDOWN])
						btnstate |= (1 << OUYA_PAD_SELECT);
					if (actObj.emuSelectStart[i][EMU_SELSTA_STADOWN])
						btnstate |= (1 << OUYA_PAD_START);
				}
				input[(i*5)+0] = btnstate;
				input[(i*5)+1] = (int)(c.getAxisValue(actObj.keyBinds[i][OUYA_AXIS_LX]) * 127) + 128;
				input[(i*5)+2] = (int)(c.getAxisValue(actObj.keyBinds[i][OUYA_AXIS_LY]) * 127) + 128;
				input[(i*5)+3] = (int)(c.getAxisValue(actObj.keyBinds[i][OUYA_AXIS_RX]) * 127) + 128;
				input[(i*5)+4] = (int)(c.getAxisValue(actObj.keyBinds[i][OUYA_AXIS_RY]) * 127) + 128;
			} else {
				//~ if (DEBUG)
					//~ Log.e(TAG, "OuyaController.getControllerByPlayer(" + i + ") returned null!");
				input[(i*5)+0] = 0;
				input[(i*5)+1] = 127;
				input[(i*5)+2] = 127;
				input[(i*5)+3] = 127;
				input[(i*5)+4] = 127;
			}
		}
        //~ if (DEBUG)
			//~ Log.d(TAG, "Returning input btnstate: " + btnstate);
		return input;
	}

	private static final int  OUYA_APP_STRING_NOBIOS		= 0;
	private static final int  OUYA_APP_STRING_NOEMPTYHLE	= 1;
	private static final int  OUYA_APP_STRING_SOME_STUFF	= 2;

	public static String getAppString(int which) {
		switch (which) {
			case OUYA_APP_STRING_NOBIOS:
				return actObj.getString(R.string.error_nobios);
			case OUYA_APP_STRING_NOEMPTYHLE:
				return actObj.getString(R.string.error_noemptyhle);
			case OUYA_APP_STRING_SOME_STUFF:
			default:
				return "";
		}
	}

    public static native void nativeOnStart();
    public static native void nativeOnResume();
    public static native void nativeOnPause();
    public static native void nativeOnStop();
    public static native void nativeSetSurface(Surface surface);
    public static native void nativeSetMessage(String msg, int frames);
    public static native void nativePrefsChanged();
    public static native void nativeEmuPauseUnpause(boolean which);
    public static native void nativeEmuLoadGame(String path);
    public static native void nativeEmuUnloadGame();
    public static native void nativeEmuResetGame();
    public static native void nativeEmuChangeDisc(String path);
    public static native void nativeEmuSaveLoad(boolean which, int slot);

    
    static {
        if (DEBUG)
			Log.d(TAG, "Loading native library...");
        System.loadLibrary("pcsx-rearmed-ouya");
    }
    
    public class MenuAdapter<T> extends ArrayAdapter<T> {
		public MenuAdapter(Context context, int textViewResourceId, T[] objects) {
			super(context, textViewResourceId, objects);
		}
   		@Override 
		public View getView(int position, View convertView, ViewGroup parent) {
			TextView textview = (TextView) super.getView(position, convertView, parent);
			textview.setTypeface(PCSX_ReARMed.gameFont);
			return textview;
		}
	}
	
	public class KeybindDialog extends AlertDialog {
		public boolean gettingKey;
		public int inputCode, player, keybindTarget;

		public KeybindDialog(Context context) {
			super(context);
		}
   		@Override
   		public boolean onGenericMotionEvent(MotionEvent event) {
			if ((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0)
				return false;
			if (OuyaController.getPlayerNumByDeviceId(event.getDeviceId()) != player)
				return false;
			if (!gettingKey) {
				inputCode = -1;
				List<InputDevice.MotionRange> mrlist = event.getDevice().getMotionRanges();
				float axisval, flatval;
				for (InputDevice.MotionRange mr : mrlist) {
					axisval = event.getAxisValue(mr.getAxis());
					//~ flatval = mr.getFlat();
					//~ flatval = OuyaController.STICK_DEADZONE;
					//~ if (axisval > flatval || axisval < (0 - flatval)) {
						//~ inputCode = mr.getAxis();
						//~ break;
					//~ }
					if (axisval == 1.0 || axisval == -1.0) {
						inputCode = mr.getAxis();
						break;
					}
				}
				if (inputCode > -1) {
					dismiss();
					return true;
				}
			}
			return false;
		}
		@Override
		public boolean onKeyDown(int keyCode, KeyEvent event) {
			//~ if (DEBUG)
				//~ Log.d("KeybindDialog", "keyCode: " + keyCode);
			if (OuyaController.getPlayerNumByDeviceId(event.getDeviceId()) != player) {
				//~ if (DEBUG)
					//~ Log.d("KeybindDialog", "Got input from player " + OuyaController.getPlayerNumByDeviceId(event.getDeviceId()) + " instead of player " + player);
				return true;
			}
			if (gettingKey) {
				inputCode = keyCode;
				dismiss();
			}
			return true;
		}
	}
}
