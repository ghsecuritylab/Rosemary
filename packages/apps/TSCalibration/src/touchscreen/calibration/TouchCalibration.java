/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

package touchscreen.calibration;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.os.SystemProperties;

public class TouchCalibration extends Activity {
    // Setting up JNI
    static {
        try {
            Log.i("JNI", "Trying to load libts.so");
            System.loadLibrary("ts");
            Log.i("JNI","tslib: loaded libts\n");
        }
        catch (UnsatisfiedLinkError ule) {
            Log.e("JNI", "WARNING: Could not load libts.so :" + ule.toString());
        }
    }

    //From TSLIB (libts.so)
    public static native int calibrateAndroid(int incomingCoords[]);
    public static String defaultPointercalValues = "1 0 0 0 1 0 1" + "\n";

    final private static boolean DEBUG = true;

    private static String mQuitString;
    private static String mPreInstruc;
    private static int mIntroState;

    final private static String TAG = "TouchCalibration";
    final private static int NEED_REBOOT_STATE = 0;
    final private static int CAN_CALIBRATION = 1;
    final private static int STEP_0 = 0;
    final private static int STEP_1 = 1;
    final private static int STEP_2 = 2;
    final private static int STEP_3 = 3;
    final private static int STEP_4 = 4;
    final private static int STEP_5 = 5;
    final private static int STEP_6 = 6;
    private static int Orientation = 0;
    private static int Rotation = 0;
    private static int Windex = 0;
    private static int CanvasHeight = 0;
    private static int CanvasWidth = 0;

    final private static int mPtCount = 5;
    final private static int mPtsLength = 2;
    private static float[] mPts;
    private static float[] mPts_ref;
    private static float[] mResultPts = new float[mPtCount * mPtsLength];
    private static float[] tempPts = new float[mPtsLength];

    private static boolean QUIT = false;
    private static int STEP = STEP_0;

    private static int CalThreshold = 70;
    private static int CalRetryNum = 3;

    final private static int maxSample = 128;

    /*
     * File Creation for storing the calibration file
     */
	public static final String filePath = "/data/data/touchscreen.calibration/files/pointercal";
	private final File calibrationFile = new File(filePath);
	protected FileWriter fileWriter;

    private FileOutputStream fos;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

		// Disable immersive mode
		setImmersive(true);

        // Remove title area
        requestWindowFeature(Window.FEATURE_NO_TITLE);
		//getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
        //                    WindowManager.LayoutParams.FLAG_FULLSCREEN);

		/*
		View decorView = getWindow().getDecorView();
		int uiOptions = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
              | View.SYSTEM_UI_FLAG_FULLSCREEN;
		decorView.setSystemUiVisibility(uiOptions);
		*/
		final int flags = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
							| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
							| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
							| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

        getWindow().getDecorView().setSystemUiVisibility(flags);
		final View decorView = getWindow().getDecorView();
		decorView.setOnSystemUiVisibilityChangeListener(new View.OnSystemUiVisibilityChangeListener() {
			@Override
			public void onSystemUiVisibilityChange(int visibility) {
				// TODO Auto-generated method stub
				if( (visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0 ){
					decorView.setSystemUiVisibility(flags);
				}
			}

		});

//		SystemProperties.set("persist.calibration.state","start");

		// Get the Text from R.string
		mQuitString = getApplicationContext().getString(R.string.quit);
		mPreInstruc = getApplicationContext().getString(R.string.instruc);

		super.setContentView(R.layout.intro);

/*
		try{
			byte[] defaultPointercal = new byte[20];
//			String defaultPointercalValues = "1 0 0 0 1 0 1" + "\n";
			String defaultPointercalValues = "98 519 946 533 931 68 91 65 508 294 50 50 974 50 974 550 50 550 512 300" + "\n";
			defaultPointercal = defaultPointercalValues.getBytes();
			fos = this.openFileOutput("pointercal", MODE_WORLD_READABLE);
			fos.write(defaultPointercal);
		}catch (Exception e){
			Log.e(TAG, "Exception Occured: Trying to default pointercal: " +
					e.toString());
			Log.e(TAG, "Finishing the Application");
			super.finish();
		}
*/
		CalRetryNum = 3;
	}

	@Override
		public void setContentView(View view) {
			super.setContentView(view);
		}


	@Override
		public void onResume(){
			//Reset the app state since points may not be accurate
			//Reset to have consecutive points.
			QUIT = false;
			STEP = STEP_0;
			super.setContentView(R.layout.intro);
			super.onResume();
		}

	@Override
		public void onDestroy(){
			SystemProperties.set("persist.calibration.state","done");
			super.onDestroy();
		}

		public float checkPointer(int posFlag, float pos) {
			float retPos = 0;

//			if ((pos <= (tempPts[posFlag] + 25)) && (pos >= (tempPts[posFlag] - 25)))
				retPos =  pos;
//			else
//				retPos = tempPts[posFlag];

			Log.d(TAG, "tempPts["+posFlag+"] = " + tempPts[posFlag]+" | retPos = " + retPos);

			return retPos;
		}

	/*
	 * Catch and handle Touch events from the device
	 */
	@Override public boolean onTouchEvent(MotionEvent event) {
		final int eventAction = event.getAction();

		Log.e(TAG,"Touch event = " + eventAction + "  STEP = " + STEP);
	/*
		if(STEP == STEP_0 ){
			STEP = STEP_1;
            Orientation = getResources().getConfiguration().orientation;
            Rotation = this.getWindowManager().getDefaultDisplay().getRotation();

            super.setContentView(new CalibrationView(this));

		}
	*/

		if (eventAction == MotionEvent.ACTION_UP){

	        if(STEP == STEP_0 ){
				STEP = STEP_1;
				Orientation = getResources().getConfiguration().orientation;
				Rotation = this.getWindowManager().getDefaultDisplay().getRotation();

				super.setContentView(new CalibrationView(this));
				return true;
			}

			Log.d(TAG, "Display rotation = " + Rotation);
			if (STEP != STEP_0){
				//Retrieve Data from Event
				if(Rotation == 0){
					mResultPts[(STEP -1) * mPtsLength] = checkPointer(0, event.getX());
					mResultPts[(STEP -1) * mPtsLength + 1] = checkPointer(1, event.getY());
				}
				else if(Rotation == 1){
					mResultPts[(STEP -1) * mPtsLength] = checkPointer(0, CanvasHeight - event.getY());
					mResultPts[(STEP -1) * mPtsLength + 1] = checkPointer(1, event.getX());
				}
				else if(Rotation == 2){
					mResultPts[(STEP -1) * mPtsLength] = checkPointer(0, CanvasWidth - event.getX());
					mResultPts[(STEP -1) * mPtsLength + 1] = checkPointer(1, CanvasHeight - event.getY());
				}
				else if(Rotation == 3){
					mResultPts[(STEP -1) * mPtsLength] = checkPointer(0, event.getY());
					mResultPts[(STEP -1) * mPtsLength + 1] = checkPointer(1, CanvasWidth - event.getX());
				}

				switch(STEP){
					case STEP_0:
						STEP = STEP_1;
						break;
					case STEP_1:
						STEP = STEP_2;
						break;
					case STEP_2:
						STEP = STEP_3;
						break;
					case STEP_3:
						//Checking to see which calibration method is used
						if (mPtCount == 3)
							QUIT = true;
						else
							STEP = STEP_4;
						break;
					case STEP_4:
						STEP = STEP_5;
						break;
					case STEP_5:
						QUIT = true;
						break;
					default:
						// Do nothing
						break;
				}
				if((STEP == STEP_3 || STEP == STEP_5) && QUIT) {
					super.setContentView(R.layout.exit);

					int[] params = new int[20];
					for (int i =0; i < (params.length / 2); i+=2){
						params[i] = (int)mResultPts[i]; //x
						params[i+1] = (int)mResultPts[i+1]; //y
						params[i+10] = (int) mPts_ref[i]; //reference x
						params[i+11] = (int) mPts_ref[i+1]; // reference y

						Log.d(TAG," debug, X[" + Integer.toString(i) + "] is="
								+ Integer.toString((int)mResultPts[i]));
						Log.d(TAG," debug, Y[" + Integer.toString(i+1) + "] is="
								+ Integer.toString((int)mResultPts[i+1]));
						Log.d(TAG," debug, refX[" + Integer.toString(i) + "] is="
								+ Integer.toString((int)mPts[i]));
						Log.d(TAG," debug, refY[" + Integer.toString(i+1) + "] is="
								+ Integer.toString((int)mPts[i+1]));
					}

					boolean confirm = false;

					// Check PTS
					// 1'st : compare x[0] and x[6]
					if ((((mResultPts[0]-mResultPts[6]) >= 0) && ((mResultPts[0]-mResultPts[6]) < 20)) || (((mResultPts[6]-mResultPts[0]) >= 0) && ((mResultPts[6]-mResultPts[0]) < 20))) {
						// 2'nd : compare x[2] and x[4]
						if ((((mResultPts[2]-mResultPts[4]) >= 0) && ((mResultPts[2]-mResultPts[4]) < 20)) || (((mResultPts[4]-mResultPts[2]) >= 0) && ((mResultPts[4]-mResultPts[2]) < 20))) {
							// 3'rd : compare y[1] and y[3]
							if ((((mResultPts[1]-mResultPts[3]) >= 0) && ((mResultPts[1]-mResultPts[3]) < 20)) || (((mResultPts[3]-mResultPts[1]) >= 0) && ((mResultPts[3]-mResultPts[1]) < 20))) {
								// 4'th : compare y[5] and y[7]
								if ((((mResultPts[5]-mResultPts[7]) >= 0) && ((mResultPts[5]-mResultPts[7]) < 20)) || (((mResultPts[7]-mResultPts[5]) >= 0) && ((mResultPts[7]-mResultPts[5]) < 20))) {
									// 5'th : compare x[8] and refX[8]
									if ( (((mResultPts[8]-mPts[8]) >= 0) && ((mResultPts[8]-mPts[8]) < 20)) || (((mPts[8]-mResultPts[8]) >= 0) && ((mPts[8]-mResultPts[8]) < 20))) {
										// 6'th : compare y[9] and refY[9]
										if ( (((mResultPts[9]-mPts[9]) >= 0) && ((mResultPts[9]-mPts[9]) < 20)) || (((mPts[9]-mResultPts[9]) >= 0) && ((mPts[9]-mResultPts[9]) < 20)))
											confirm = true;
									}
								}
							}
						}
					}

					try{

						if (confirm == true)
							fos = this.openFileOutput("pointercal", MODE_WORLD_READABLE);

						String rawValues = new String();

						for (int i =0; i < params.length; i++){

							if((Orientation == 2 && Rotation == 0) || (Orientation == 2 && Rotation == 1))
								Windex = i;


							if((Orientation == 1 && Rotation == 1) || (Orientation == 1 && Rotation == 2))
							{
								if(i==6) Windex = 0; else if(i==7) Windex = 1;
								else if(i==16) Windex = 10; else if(i==17) Windex = 11;
								else if(i>7 && i<10) Windex = i; else if(i>17) Windex = i;
								else
									Windex = i+2;
							}

							if((Orientation == 2 && Rotation == 2) || (Orientation == 2 && Rotation == 3))
							{
								if(i==4) Windex = 0; else if(i==5) Windex = 1;
								else if(i==14) Windex = 10; else if(i==15) Windex = 11;
								else if(i==6) Windex = 2; else if(i==7) Windex = 3;
								else if(i==16) Windex = 12; else if(i==17) Windex = 13;
								else if(i>7 && i<10) Windex = i; else if(i>17) Windex = i;
								else
									Windex = i+4;
							}

							if((Orientation == 1 && Rotation == 3) || (Orientation == 1 && Rotation == 0))
							{
								if(i==2) Windex = 0; else if(i==3) Windex = 1;
								else if(i==12) Windex = 10; else if(i==13) Windex = 11;
								else if(i==4) Windex = 2; else if(i==5) Windex = 3;
								else if(i==14) Windex = 12; else if(i==15) Windex = 13;
								else if(i==6) Windex = 4; else if(i==7) Windex = 5;
								else if(i==16) Windex = 14; else if(i==17) Windex = 15;
								else if(i>7 && i<10) Windex = i; else if(i>17) Windex = i;
								else
									Windex = i+6;
							}

							rawValues += Integer.toString(params[Windex]);
							rawValues += " ";
						}

						// Save data
						if (confirm == true)
							fos.write(rawValues.getBytes());
					}
					catch (Exception e){
						Log.e(TAG, "Exception Occured: Trying to change to World Readable: " +
								e.toString());
						Log.e(TAG, "Finishing the Application");
						super.finish();
					}

					if (fos != null){
						try{fos.close();}catch(Exception e){
							Log.d("TAG", "Exception Occured While Trying to Close and Save "
									+ e.toString());
						};
					}

					SystemProperties.set("persist.calibration.state","done");
					this.finish();
					super.finish();

					char[] pointercalBuffer = new char[100];
					String pointercalValues = "";
					int count;

					try{
						if (confirm == true) {
							FileReader rd = new FileReader(TouchCalibration.filePath);
							count = rd.read(pointercalBuffer, 0, 100);
							for(int i=0; i<count; i++){
								pointercalValues += pointercalBuffer[i];
							}
							Log.i("This is the pointercal", pointercalValues);
							rd.close();
						}
					}
					catch(Exception e){
						Log.e(TAG,
								"Pointercal file unable to be created correctly:" + e.toString());
					}
				}
				else
					super.setContentView(new CalibrationView(this));
			}
		}
		return true;
	}

	/*
	 * Helper function for the alert dialog
	 */
	private void checkQuit(){
		if(QUIT)
			super.finish();
	}

	/*
	 * More Organized FileWriter Append with try catch
	 */
	private boolean appendToFile(String inputString){
		try{
			fileWriter.append(inputString);
		}
		catch(Exception e){
			Log.e("TAG", "Exception Occured While Trying to Append to File: " +
					e.toString());
			return false;
		}
		return true;
	}

	/*
	 * View Class: allow canvas drawing on top of the View to display to users
	 */
	private static class CalibrationView extends View {
		private Paint mPaint = new Paint();
		private float[] mInstructionPts;
		private float[] mExitPts;

		final int mTextSize = 15;

		public CalibrationView(Context context) {
			super(context);
		}

		private void buildCalibrationPoints(int cHeight, int cWidth){
			mPts = new float[mPtCount * 2];
			mPts_ref = new float[mPtCount * 2];
			if (mPtCount == 3){
				mPts[0] = cWidth * 15 / 100;
				mPts[1] = cHeight * 15 / 100;
				mPts[2] = cWidth / 2;
				mPts[3] = cHeight * 85 / 100;
				mPts[4] = cWidth * 85 / 100;
				mPts[5] = cHeight / 2;
			}
			else if(mPtCount == 5){

				//Points are suggested by tslib

				mPts[0] = 50;             // Top Left
				mPts[1] = 50;
				mPts[2] = cWidth - 50; //Top Right
				mPts[3] = 50;
				mPts[4] = cWidth - 50; // Bot Right
				mPts[5] = cHeight - 50;
				mPts[6] = 50;             // Bot Left
				mPts[7] = cHeight - 50;
				mPts[8] = cWidth / 2; // Center
				mPts[9] = cHeight / 2;

				if(Rotation == 0){
					mPts_ref[0] = 50;
					mPts_ref[1] = 50;
					mPts_ref[2] = cWidth - 50;
					mPts_ref[3] = 50;
					mPts_ref[4] = cWidth - 50;
					mPts_ref[5] = cHeight - 50;
					mPts_ref[6] = 50;
					mPts_ref[7] = cHeight - 50;
					mPts_ref[8] = cWidth/2;
					mPts_ref[9] = cHeight/2;
				}
				if(Rotation == 1){
					mPts_ref[0] = cHeight - 50;
					mPts_ref[1] = 50;
					mPts_ref[2] = cHeight - 50;
					mPts_ref[3] = cWidth - 50;
					mPts_ref[4] = 50;
					mPts_ref[5] = cWidth - 50;
					mPts_ref[6] = 50;
					mPts_ref[7] = 50;
					mPts_ref[8] = cHeight/2;
					mPts_ref[9] = cWidth/2;
				}
				if(Rotation == 2){
					mPts_ref[0] = cWidth - 50;
					mPts_ref[1] = cHeight - 50;
					mPts_ref[2] = 50;
					mPts_ref[3] = cHeight - 50;
					mPts_ref[4] = 50;
					mPts_ref[5] = 50;
					mPts_ref[6] = cWidth - 50;
					mPts_ref[7] = 50;
					mPts_ref[8] = cWidth/2;
					mPts_ref[9] = cHeight/2;
				}
				if(Rotation == 3){
					mPts_ref[0] = 50;
					mPts_ref[1] = cWidth - 50;
					mPts_ref[2] = 50;
					mPts_ref[3] = 50;
					mPts_ref[4] = cHeight - 50;
					mPts_ref[5] = 50;
					mPts_ref[6] = cHeight - 50;
					mPts_ref[7] = cWidth - 50;
					mPts_ref[8] = cHeight/2;
					mPts_ref[9] = cWidth/2;
				}

			}
		}

		// Access the Calibration Points String to compare to Drivers Raw data
		private static String getCalibrationPoints(){
			String result = "";
			for (int i = 0; i < mPts.length; i+=2){
				result += "\n" + Float.toString(mPts[i]) +","+
					Float.toString(mPts[i+1]) + ";";
			}
			return result;
		}

		private void buildTextPosition(String displayString, int yPosition,
				float[] textPts) {
			for (int i = 0; i < textPts.length; i+=2){
				textPts[i] = i * 5;
				textPts[i+1] = yPosition;
			}
		}

		@Override protected void onDraw(Canvas canvas) {
			Paint paint = mPaint;
			final String calibrationInstruction = mPreInstruc +
				Integer.toString(STEP);
			CanvasHeight = canvas.getHeight();
			CanvasWidth  = canvas.getWidth();
			if (DEBUG){
				Log.d("***Canvas height *** ", Integer.toString(CanvasHeight));
				Log.d("***Canvas width *** ", Integer.toString(CanvasWidth));
			}

			SystemProperties.set("persist.calibration.state","start");

			//Build Points

			//Orientation = getResources().getConfiguration().orientation;
			//Rotation = this.getWindowManager().getDefaultDisplay().getRotation();

			//if(Rotation == 0 || Rotation == 2)
			//    buildCalibrationPoints(CanvasWidth, CanvasHeight);
			//else
			buildCalibrationPoints(CanvasHeight, CanvasWidth);

			mInstructionPts = new float[calibrationInstruction.length()*2];
			mExitPts = new float[mQuitString.length()*2];
			buildTextPosition(calibrationInstruction, CanvasHeight - mTextSize
					- 7, mInstructionPts);
			buildTextPosition(mQuitString, CanvasHeight - ( mTextSize /2 ),
					mExitPts);

			canvas.translate(0, 0);
			canvas.drawColor(Color.BLACK);

			paint.setColor(Color.WHITE);
			paint.setTextSize(mTextSize);
			canvas.drawPosText(calibrationInstruction, mInstructionPts, paint);
			canvas.drawPosText(mQuitString, mExitPts, paint);

			// Create sub array of points
			System.arraycopy(mPts, (STEP -1) * mPtsLength, tempPts, 0,
					mPtsLength);


			//Setting up and drawing targets (LARGER)
			paint.setStrokeWidth(25);
			canvas.drawCircle(tempPts[0], tempPts[1], paint.getStrokeWidth(),
					paint);

			paint.setColor(Color.RED);
			paint.setStrokeWidth(21);
			canvas.drawCircle(tempPts[0], tempPts[1], paint.getStrokeWidth(),
					paint);

			paint.setColor(Color.WHITE);
			paint.setStrokeWidth(17);
			canvas.drawCircle(tempPts[0], tempPts[1], paint.getStrokeWidth(),
					paint);

			paint.setColor(Color.RED);
			paint.setStrokeWidth(13);
			canvas.drawCircle(tempPts[0], tempPts[1], paint.getStrokeWidth(),
					paint);

			paint.setColor(Color.WHITE);
			paint.setStrokeWidth(9);
			canvas.drawCircle(tempPts[0], tempPts[1], paint.getStrokeWidth(),
					paint);

			paint.setColor(Color.RED);
			paint.setStrokeWidth(5);
			canvas.drawCircle(tempPts[0], tempPts[1], paint.getStrokeWidth(),
					paint);

			//Center Point
			paint.setColor(Color.WHITE);
			paint.setStrokeWidth(1);
			canvas.drawCircle(tempPts[0], tempPts[1], paint.getStrokeWidth(),
					paint);

			//Drawing a Cross
			paint.setStrokeWidth(2);
			canvas.drawLine(tempPts[0] + 28, tempPts[1], tempPts[0]
					- 28, tempPts[1], paint);
			canvas.drawLine(tempPts[0], tempPts[1] + 28, tempPts[0],
					tempPts[1] - 28, paint);
		}
	}
}
