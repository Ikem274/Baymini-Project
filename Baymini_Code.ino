#include <Wire.h>
#include <VL53L0X.h>
#include <Servo.h>

// -------- Emotion List --------
enum Emotion {
    Emotion_happy,
    Emotion_sad,
    Emotion_angry,
    Emotion_sleeping,
    Emotion_confused,
    Emotion_neutral
};
Emotion gCurrentEmotion = Emotion_neutral;


// -------- ToF sensor --------
VL53L0X sensor;
const int kCoverDistance = 40;        
const unsigned long kHappyCover = 1500; 
const unsigned long kSadCover = 5000;  
unsigned long gCoverStartTime = 0;
bool gIsCovered = false;


//------- Idle Timing-------
unsigned long gLastInteractionTime = 0;
const unsigned long kNeutralTimeout = 6000;
const unsigned long kSleepTimeout = 15000;
bool gWheelsRetracted = false;


// -------- Motor Pin Assignments --------
const unsigned int kBreathingMotorPin1 = 2;
const unsigned int kBreathingMotorPin2 = 3;
const unsigned int kWheel1A = 4;
const unsigned int kWheel1B = 5;
const unsigned int kWheel2A = 6;
const unsigned int kWheel2B = 7;
const unsigned int kServoPin = 10;
Servo eyebrowServo;


// -------- Piezo --------
const unsigned int kPiezoAnalogPin = A0;
const float kPiezoThreshold = 10.0;
const unsigned long kMaxTapInterval = 700;
const unsigned long kMinTapInterval = 200;
const int kRequiredTaps = 5;

unsigned long gLastTapTime = 0;
int gGoodTapCount = 0;
int gBadTapCount = 0;
float gPrevPiezoValue = 0.0;


// -------- Breathing --------
unsigned long gLastBreathTime = 0;
int gBreathStep = 0;
bool gInHold = false;
float gAmplitude = 1.0;
int gTimePeriod = 1000;
int gHoldDelay = 200;
int gSteps = 0;
float gPhaseIncrement = 0.0;


// -------- Emotion Level System --------
const int kMaxEmotionLevel = 2;
// every 5 interactions increases level
const int kLevelTriggerCount = 5; 
unsigned long gLevel2EmotionStartTime = 0;
bool gIsLevel2EmotionActive = false;

int gHappyLevel = 0;
int gSadLevel = 0;
int gAngryLevel = 0;

int gHappyCounter = 0;
int gSadCounter = 0;
int gAngryCounter = 0;


// -------- Servo Expressions --------
int gNeutralAngle = 90;
int gHappyAngle = 95;
int gSadAngle = 125;
int gAngryAngle = 50;
int gConfusedAngle = 115;
int gSleepAngle = 80;



// --------- Forward & backwards wheel movement ---------
// Defined functions to call when the wheels need to be moved forward, backwards or stopped
void setAllWheelsForward(int speed) {
    digitalWrite(kWheel1A, LOW);
    analogWrite(kWheel1B, speed);
    digitalWrite(kWheel2A, LOW);
    analogWrite(kWheel2B, speed);
}

void setAllWheelsBackward(int speed) {
    analogWrite(kWheel1A, speed);
    digitalWrite(kWheel1B, LOW);
    analogWrite(kWheel2A, speed);
    digitalWrite(kWheel2B, LOW);
}
void stopAllWheels() {
    digitalWrite(kWheel1A, LOW);
    analogWrite(kWheel1B, 0);
    digitalWrite(kWheel2A, LOW);
    analogWrite(kWheel2B, 0);
}



// --------- Setting Breathing and Eyebrows ---------
// Set cases to switch to depending on what emotion it's on
// Set motor inputs as sine waves and varied them depending on the emotion
// Switched the eyebrow angle also depending on the emotion

void updateBreathAndEyebrowParams() {
    switch (gCurrentEmotion) {
        case Emotion_happy:
            gAmplitude = 1.0;
            gTimePeriod = 1000 - (gHappyLevel * 150);   // faster per level
            gHoldDelay = 200;
            eyebrowServo.write(gHappyAngle + gHappyLevel * 10);
            break;
        case Emotion_sad:
            gAmplitude = 0.6;
            gTimePeriod = 2000 + (gSadLevel * 300);     // slower per level
            gHoldDelay = 500 + (gSadLevel * 150);
            eyebrowServo.write(gSadAngle + gSadLevel * 15);
            break;
        case Emotion_angry:
            gAmplitude = 1.0;
            gTimePeriod = 500 - (gAngryLevel * 80);     // more frantic per level
            gHoldDelay = 100;
            eyebrowServo.write(gAngryAngle - gAngryLevel * 10);
            break;
        case Emotion_sleeping:
            gAmplitude = 0;
            gTimePeriod = 0;
            gHoldDelay = 0;
            eyebrowServo.write(gSleepAngle);
            break;
        case Emotion_confused:
            gAmplitude = 0.9;
            gTimePeriod = 1300;
            gHoldDelay = 150;
            eyebrowServo.write(gConfusedAngle);
            break;
        default:
            gAmplitude = 0.8;
            gTimePeriod = 1500;
            gHoldDelay = 300;
            eyebrowServo.write(gNeutralAngle);
            break;
    }
    gSteps = gTimePeriod / 50;
    gPhaseIncrement = 2.0 * PI / gSteps;
}


//--------- Emotion Level-Up ---------
// Function increases the "level" of a given emotion (happy, sad, or angry) 
// + ensures the level does not exceed kMaxEmotionLevel.
void increaseEmotionLevel(int &level, int &counter) {
    // Increment the counter each time the function is called
    counter++;

    // If the counter reaches the required number of interactions for a level-up
    if (counter >= kLevelTriggerCount) {
        // Reset the counter for the next cycle
        counter = 0;
        // Increase the emotion level by 1
        level++;

        // Make sure the level does not exceed the maximum allowed
        if (level > kMaxEmotionLevel)
            level = kMaxEmotionLevel;

        Serial.print("New level = ");
        Serial.println(level);
    }
}


//------ Emotion Reset ---------
void resetAllEmotionLevels() {
    //function to reset the emotions back to 0
    gHappyLevel = 0;
    gSadLevel = 0;
    gAngryLevel = 0;

    gHappyCounter = 0;
    gSadCounter = 0;
    gAngryCounter = 0;
}




// -------- Setup --------
void setup() {
    Serial.begin(9600);
    Wire.begin();

    // Motor
    pinMode(kBreathingMotorPin1, OUTPUT);
    pinMode(kBreathingMotorPin2, OUTPUT);
    pinMode(kWheel1A, OUTPUT);
    pinMode(kWheel1B, OUTPUT);
    pinMode(kWheel2A, OUTPUT);
    pinMode(kWheel2B, OUTPUT);
    pinMode(kServoPin, OUTPUT);
    eyebrowServo.attach(kServoPin);

    // Piezo
    pinMode(kPiezoAnalogPin, INPUT);

    // VL53L0X
    if (!sensor.init()) {
        Serial.println("VL53L0X failed!");
        while (1);
    }
    sensor.setTimeout(500);
    sensor.startContinuous();

    gLastBreathTime = millis();
    updateBreathAndEyebrowParams();
}





//------- Main Loop -------
void loop() {
    unsigned long now = millis();

    // --------- Time of Flight Sensor Code -------
    // Set distance to the ToF's sensor reading
    uint16_t distance = sensor.readRangeContinuousMillimeters();
    //Checks to see if sensor failed to return data or not
    if (!sensor.timeoutOccurred()) {

        //Check if the distance is less than 40mm (Acts like its holding its breath)
        if (distance < kCoverDistance) {
            //runs once to check when hand is first detected
            //Sets covered to true and updates Baymini into a confused state
            if (!gIsCovered) {
                gCoverStartTime = now;
                gIsCovered = true;
                gCurrentEmotion = Emotion_confused;
                updateBreathAndEyebrowParams();
                gLastInteractionTime = now;
            }
            // Define the time for how long the sensor is covered for
            unsigned long coverTime = now - gCoverStartTime;
            // If the sensor is covered for more than 3 seconds set emotion to Sad
            // For every 3 seconds the sensor is covered for in a row, make it sadder and sadder
            if (coverTime >= 3000) {
                // Set sad to Level 0 for <3s, Level 1 for 3-6s 2 for 6-9s
                int newSadLevel = (coverTime / 3000) - 1; 
                if (newSadLevel > 2) 
                    newSadLevel = 2;

                if (gSadLevel != newSadLevel || gCurrentEmotion != Emotion_sad) {
                    gSadLevel = newSadLevel;
                    gCurrentEmotion = Emotion_sad;
                    updateBreathAndEyebrowParams();
                    gLastInteractionTime = now;
                }
            }
        }

        else {
            // Check if hand is removed from the sensor
            if (gIsCovered) {
                unsigned long duration = now - gCoverStartTime;
                // if the sensor is covered for a short time, act like Peekaboo and become happy level 2
                if (duration <= kHappyCover) {
                    gCurrentEmotion = Emotion_happy;
                    gHappyLevel = 2; 
                    updateBreathAndEyebrowParams();
                    gLastInteractionTime = now;
                }
                //if anything else is happening set the emotion to neutral
                else {
                    gCurrentEmotion = Emotion_neutral;
                    updateBreathAndEyebrowParams();
                }
            }
            //set sensor to no longer being covered
            gIsCovered = false;
        }
    }



    // -------- Piezo disk tap detection -------
    float piezoValue = analogRead(kPiezoAnalogPin);
    // Piezo Disk senses a tap when above a certain threshold so accidental piezo values aren't included
    // Included previous Value so it doesn't take one tap as multiple due to the piezo disk's read rate
    if (piezoValue > kPiezoThreshold && gPrevPiezoValue <= kPiezoThreshold) {
        //define tap interval timing
        unsigned long tapInterval = now - gLastTapTime;
        //Check if the tap interval is at right tempo
        if (tapInterval >= kMinTapInterval && tapInterval <= kMaxTapInterval) {
            // increase good tap count
            gGoodTapCount++;
            // reset bad tap count 
            gBadTapCount = 0;
            //Check if 5 Good taps happen in a row to increase happiness level
            if (gGoodTapCount == kRequiredTaps) {
                // Set emotion to happy, increase level & Update breathing
                gCurrentEmotion = Emotion_happy;
                increaseEmotionLevel(gHappyLevel, gHappyCounter);
                updateBreathAndEyebrowParams();
                //Reset Good tap Count to 0 so code can be reused
                gGoodTapCount = 0;
                gLastInteractionTime = now;
                }
            }
        else {
            // If tap interval is not at the right tempo, increase BadTapCount
            gBadTapCount++;
            // Set Good Tap count to 0
            gGoodTapCount = 0;
            // Check if 5 bad taps happen in a row to increase the angriness level
            if (gBadTapCount == kRequiredTaps) {
                // Set emotion to angry, increase level & update breathing
                gCurrentEmotion = Emotion_angry;
                increaseEmotionLevel(gAngryLevel, gAngryCounter);
                updateBreathAndEyebrowParams();
                //Reset Bad Count to 0 to reuse the code
                gBadTapCount = 0;
                gLastInteractionTime = now;
            }

        }
        // set the last tap time to the current time
        gLastTapTime = now;
    }
    // redefine the previous piezo disk value
    gPrevPiezoValue = piezoValue;



    // ------- Inactivity --------
    //Check if Baymini has been interacted with in the last 6 seconds
    if ((now - gLastTapTime > kNeutralTimeout) && (now - gLastInteractionTime > kNeutralTimeout) && !gIsCovered) {
        //Set Emotion to neutral
        gCurrentEmotion = Emotion_neutral;
        updateBreathAndEyebrowParams();
    }
    // Check if Baymini has been interacted with in the last 15
    if ((now - gLastTapTime > kSleepTimeout) && (now - gLastInteractionTime > kSleepTimeout) && !gIsCovered) {
        gCurrentEmotion = Emotion_sleeping;
        updateBreathAndEyebrowParams();

    }




    // Start level 2 only once
    if (!gIsLevel2EmotionActive) {
        //if its at happy level 2, move closer towards you (forward)
        if (gHappyLevel == 2 && gCurrentEmotion == Emotion_happy) {
            setAllWheelsForward(150);
            gLevel2EmotionStartTime = now;
            gIsLevel2EmotionActive = true;
        }
        // if its angry level 2, move away from you (backwards)
        if (gAngryLevel == 2 && gCurrentEmotion == Emotion_angry) {
            setAllWheelsBackward(150);
            gLevel2EmotionStartTime = now;
            gIsLevel2EmotionActive = true;
        }
        // if it's sad level 2 AKA holding its breath move away from you (Backwards)
        if (gSadLevel == 2 && gCurrentEmotion == Emotion_sad) {
            setAllWheelsBackward(150);
            gLevel2EmotionStartTime = now;
            gIsLevel2EmotionActive = true;
        }
    }


        // If level 2 emotion is active, check if 1 second has passed
    if (gIsLevel2EmotionActive) {
        if (now - gLevel2EmotionStartTime >= 2000) {
            // Stop wheels and reset emotion/levels
            stopAllWheels();
            gCurrentEmotion = Emotion_neutral;
            resetAllEmotionLevels();
            updateBreathAndEyebrowParams();
            gIsLevel2EmotionActive = false;
            }
        }




    // ---------- Motor / Breathing Motion ---------
    // This variable keeps track of the current phase in the breathing cycle (sine wave).
    static float phase = 0.0;

    // If we are not in the "hold" phase
    if (!gInHold) {
        // Update the breathing position every 10 milliseconds
        if (now - gLastBreathTime >= 10) {
            // Calculate the current position on the sine wave for smooth breathing motion.
            // gAmplitude scales the wave, phase determines position, PI/2 makes it start at minimum (exhale).
            float positiveSine = gAmplitude * sin(phase - (PI / 2));
            // Clamp the value between 0.0 and 1.0 to avoid negative PWM or values above max
            positiveSine = constrain(positiveSine, 0.0, 1.0);

            // Output the calculated value as a PWM signal to the breathing motor.
            // Multiply by 255 to scale to Arduino's analogWrite range (0-255).
            analogWrite(kBreathingMotorPin1, (int)(positiveSine * 255));
            // Set the other motor pin LOW
            digitalWrite(kBreathingMotorPin2, LOW);

            // Move to the next step in the sine wave
            phase += gPhaseIncrement;
            gBreathStep++;
            //update the time between breaths
            gLastBreathTime = now;
        }
        // If we've completed all the steps for one full breath cycle (inhale + exhale)
        if (gBreathStep >= gSteps) {
            // Reset the breath step and phase for the next cycle
            gBreathStep = 0;
            phase = 0.0;
            // Enter the "hold" phase
            gInHold = true;
            gLastBreathTime = now;
        }
    }

    else {
        // If the hold delay has passed, exit the hold phase and start a new breath cycle
        if (now - gLastBreathTime >= gHoldDelay) {
            gInHold = false;
        }
    }



    // ------- Debugging --------
    Serial.print("Dist: "); Serial.print(distance);
    Serial.print("\tEmotion: "); Serial.println(gCurrentEmotion);
    Serial.print("Piezo: ");
    Serial.print(piezoValue);
    Serial.print("\tGood: ");
    Serial.print(gGoodTapCount);
    Serial.print("\tBad: ");
    Serial.print(gBadTapCount);
}