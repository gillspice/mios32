/* -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*- */
// $Id: UploadWindow.cpp 1190 2011-04-24 23:20:11Z tk $
/*
 * Upload Window Component
 *
 * ==========================================================================
 *
 *  Copyright (C) 2010 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#include "UploadWindow.h"
#include "MiosStudio.h"

#include <list>


#define TIMER_LOAD_HEXFILE             1
#define TIMER_LOAD_HEXFILE_AND_UPLOAD  2
#define TIMER_UPLOAD                   3
#define TIMER_DELAYED_PROGRESS_OFF     4
#define TIMER_QUERY_CORE               5


//==============================================================================
UploadWindow::UploadWindow(MiosStudio *_miosStudio)
    : miosStudio(_miosStudio)
    , progress(0)
    , waitUploadRequestMessagePrint(0)
{
	addAndMakeVisible(fileChooser = new FilenameComponent (T("hexfile"),
                                                           File::nonexistent,
                                                           true, false, false,
                                                           "*.hex",
                                                           String::empty,
                                                           T("(choose a .hex file to upload)")));
	fileChooser->addListener(this);
	fileChooser->setBrowseButtonText(T("Browse"));
    fileChooser->setEnabled(true);

    addAndMakeVisible(queryButton = new TextButton(T("Query Button")));
    queryButton->setButtonText(T("Query"));
    queryButton->addButtonListener(this);

    addAndMakeVisible(deviceIdLabel = new Label(T("Device ID"), T("Device ID")));
    deviceIdLabel->setJustificationType(Justification::centred);
    
    addAndMakeVisible(deviceIdSlider = new Slider(T("Device ID")));
    deviceIdSlider->setRange(0, 127, 1);
    deviceIdSlider->setValue(miosStudio->uploadHandler->getDeviceId()); // restored from setup file
    deviceIdSlider->setSliderStyle(Slider::IncDecButtons);
    deviceIdSlider->setTextBoxStyle(Slider::TextBoxAbove, false, 80, 20);
    deviceIdSlider->setDoubleClickReturnValue(true, 0);
    deviceIdSlider->addListener(this);

    addAndMakeVisible(uploadQuery = new LogBox(T("Upload Query")));
    uploadQuery->addEntry(Colours::grey, T("Waiting for first query."));

    addAndMakeVisible(uploadStatus = new LogBox(T("Upload Status")));

    addAndMakeVisible(startButton = new TextButton(T("Start Button")));
    startButton->setButtonText(T("Start"));
    startButton->addButtonListener(this);
    startButton->setEnabled(false);

    addAndMakeVisible(stopButton = new TextButton(T("Stop Button")));
    stopButton->setButtonText(T("Stop"));
    stopButton->addButtonListener(this);
    stopButton->setEnabled(false);

    addAndMakeVisible(progressBar = new ProgressBar(progress));

    // restore settings
    PropertiesFile *propertiesFile = ApplicationProperties::getInstance()->getCommonSettings(true);
    if( propertiesFile ) {
        String recentlyUsedHexFiles = propertiesFile->getValue(T("recentlyUsedHexFiles"), String::empty);
        // seems that Juce doesn't provide a split function?
        StringArray recentlyUsedHexFilesArray;
        int index = 0;
        while( (index=recentlyUsedHexFiles.indexOfChar(';')) >= 0 ) {
            recentlyUsedHexFilesArray.add(recentlyUsedHexFiles.substring(0, index));
            recentlyUsedHexFiles = recentlyUsedHexFiles.substring(index+1);
        }
        if( recentlyUsedHexFiles != String::empty )
            recentlyUsedHexFilesArray.add(recentlyUsedHexFiles);
        fileChooser->setRecentlyUsedFilenames(recentlyUsedHexFilesArray);

        fileChooser->setDefaultBrowseTarget(propertiesFile->getValue(T("defaultFile"), String::empty));
    }

    setSize(400, 200);
}

UploadWindow::~UploadWindow()
{
}

//==============================================================================
void UploadWindow::paint (Graphics& g)
{
    g.fillAll(Colours::white);
}

void UploadWindow::resized()
{
    fileChooser->setBounds(4, 4, getWidth()-8, 24);

    int middleX = getWidth()/2 - 75;

    int buttonY = 4+56;
    int buttonWidth = 72;
    deviceIdLabel->setBounds(4, 30, buttonWidth, 40);
    deviceIdSlider->setBounds(4, buttonY+0*36, buttonWidth, 40);
    queryButton->setBounds(4, buttonY+1*36+10, buttonWidth, 24);

    int uploadQueryX = 4+buttonWidth+4;
    int uploadQueryWidth = middleX - 4 - uploadQueryX;
    uploadQuery->setBounds(uploadQueryX, 4+30, uploadQueryWidth, getHeight()-(4+24+4+10));

    int uploadStatusX = middleX + 4;
    int uploadStatusY = 4+30;
    int uploadStatusWidth = getWidth() - 4 - buttonWidth - 4 - uploadStatusX;
    int uploadStatusHeight = getHeight()-(4+24+4+10+24+4);
    uploadStatus->setBounds(uploadStatusX, uploadStatusY, uploadStatusWidth, uploadStatusHeight);
    progressBar->setBounds(uploadStatusX, uploadStatusY+4+uploadStatusHeight, uploadStatusWidth, 24);

    buttonY = 4+40;
    int startStopButtonX = getWidth() - 4 - buttonWidth;
    startButton->setBounds(startStopButtonX, buttonY+0*36, buttonWidth, 24);
    stopButton->setBounds (startStopButtonX, buttonY+1*36, buttonWidth, 24);
}

void UploadWindow::buttonClicked(Button* buttonThatWasClicked)
{
    if( buttonThatWasClicked == startButton ) {
        File inFile = fileChooser->getCurrentFile();
        uploadStatus->clear();
        uploadStatus->addEntry(Colours::black, T("Reading ") + inFile.getFileName());
        uploadStart();
    } else if( buttonThatWasClicked == stopButton ) {
        uploadStop();
        uploadStatus->addEntry(Colours::red, T("Upload has been stopped by user!"));
    } else if( buttonThatWasClicked == queryButton ) {
        queryCore();
    }
}

void UploadWindow::sliderValueChanged(Slider* slider)
{
    if( slider == deviceIdSlider ) {
        uploadStop();
        miosStudio->uploadHandler->setDeviceId((uint8)slider->getValue());
    }
}

void UploadWindow::filenameComponentChanged(FilenameComponent *fileComponentThatHasChanged)
{
    if( fileComponentThatHasChanged == fileChooser ) {
        File inFile = fileChooser->getCurrentFile();

        uploadStatus->clear();
        uploadStatus->addEntry(Colours::black, T("Reading ") + inFile.getFileName());
        MultiTimer::startTimer(TIMER_LOAD_HEXFILE, 1);
        startButton->setEnabled(false); // will be enabled if file is valid

        // store setting
        PropertiesFile *propertiesFile = ApplicationProperties::getInstance()->getCommonSettings(true);
        if( propertiesFile ) {
            String recentlyUsedHexFiles = fileChooser->getRecentlyUsedFilenames().joinIntoString(";");
            propertiesFile->setValue(T("recentlyUsedHexFiles"), recentlyUsedHexFiles);
            propertiesFile->setValue(T("defaultFile"), inFile.getFullPathName());
        }
    }
}


//==============================================================================
void UploadWindow::midiPortChanged(void)
{
    miosStudio->uploadHandler->finish();
    stopButton->setEnabled(false);
    queryButton->setEnabled(true);
    deviceIdSlider->setEnabled(true);
    fileChooser->setEnabled(true);

    uploadQuery->clear();
    uploadQuery->addEntry(Colours::red, T("No response from a core yet..."));
    uploadQuery->addEntry(Colours::red, T("Check MIDI IN/OUT connections"));
    uploadQuery->addEntry(Colours::red, T("and Device ID!"));
    uploadStatus->clear();
    queryCore();
}

//==============================================================================
void UploadWindow::queryCore(void)
{
    if( !miosStudio->uploadHandler->startQuery() ) {
        uploadQuery->clear();
        uploadQuery->addEntry(Colours::red, T("Please try again later - ongoing transactions!"));
    } else {
        uploadQuery->clear();
        queryButton->setEnabled(false);
        deviceIdSlider->setEnabled(false);
        fileChooser->setEnabled(false);
        MultiTimer::startTimer(TIMER_QUERY_CORE, 10);
    }
}


//==============================================================================
void UploadWindow::uploadStart(void)
{
    progress = 0;
    waitUploadRequestMessagePrint = 0;

    startButton->setEnabled(false); // will be enabled again if file is valid
    queryButton->setEnabled(false);
    deviceIdSlider->setEnabled(false);
    fileChooser->setEnabled(false);

    uploadStatus->addEntry(Colours::grey, T("Trying to contact the core..."));

    uploadQuery->clear();
    uploadQuery->addEntry(Colours::black, T("Upload in progress..."));

    MultiTimer::startTimer(TIMER_LOAD_HEXFILE_AND_UPLOAD, 1);
}


void UploadWindow::uploadStop(void)
{
    miosStudio->uploadHandler->finish();
    startButton->setEnabled(miosStudio->uploadHandler->hexFileLoader.hexDumpAddressBlocks.size() >= 1);
    stopButton->setEnabled(false);
    queryButton->setEnabled(true);
    deviceIdSlider->setEnabled(true);
    fileChooser->setEnabled(true);
}


//==============================================================================
void UploadWindow::timerCallback(const int timerId)
{
    // always stop timer, it has to be restarted if required
    MultiTimer::stopTimer(timerId);

    // check for requested service
    if( timerId == TIMER_LOAD_HEXFILE || timerId == TIMER_LOAD_HEXFILE_AND_UPLOAD ) {
        File inFile = fileChooser->getCurrentFile();
        String statusMessage;
        if( miosStudio->uploadHandler->hexFileLoader.loadFile(inFile, statusMessage) ) {
            uploadStatus->addEntry(Colours::black, statusMessage);

            if( miosStudio->uploadHandler->hexFileLoader.hexDumpAddressBlocks.size() < 1 ) {
                uploadStatus->addEntry(Colours::red, T("ERROR: no blocks found"));
                uploadStop();
            } else {
                // display and check ranges
                if( !miosStudio->uploadHandler->checkAndDisplayRanges(uploadStatus) ) {
                    uploadStatus->addEntry(Colours::red, T("ERROR: Range check failed!"));
                    uploadStop();
                } else if( timerId == TIMER_LOAD_HEXFILE_AND_UPLOAD ) {
                    // start upload of first block
                    stopButton->setEnabled(true);
                    queryButton->setEnabled(false);
                    deviceIdSlider->setEnabled(false);
                    miosStudio->uploadHandler->startUpload();
                    MultiTimer::startTimer(TIMER_UPLOAD, 10);
                } else {
                    uploadStatus->addEntry(Colours::black, T("Press start button to begin upload."));
                    startButton->setEnabled(true);
                }
            }
        } else {
            uploadStatus->addEntry(Colours::red, T("ERROR: ") + statusMessage);
            uploadStop();
        }
    } else if( timerId == TIMER_UPLOAD ) {
        progress = (double)miosStudio->uploadHandler->currentBlock / (double)miosStudio->uploadHandler->totalBlocks;

        int busyState;
        if( (busyState=miosStudio->uploadHandler->busy()) > 0 ) {
            if( busyState == 2 && !waitUploadRequestMessagePrint ) {
                uploadStatus->addEntry(Colours::brown, T("WARNING: no response from core"));
                uploadStatus->addEntry(Colours::brown, T("Please reboot the core (e.g. turning off/on power)!"));
                uploadStatus->addEntry(Colours::brown, T("Waiting for upload request..."));
                waitUploadRequestMessagePrint = true;
            } else if( busyState == 1 && waitUploadRequestMessagePrint ) {
                uploadStatus->addEntry(Colours::brown, T("Received upload request!"));
                waitUploadRequestMessagePrint = false;
            }

            // wait again for 10 mS - the uploader provides an own timeout mechanism
            MultiTimer::startTimer(TIMER_UPLOAD, 10);
        } else {
            String errorMessage = miosStudio->uploadHandler->finish();

            if( errorMessage != String::empty ) {
                // TODO: word-wrapping required here for multiple lines
                uploadStatus->addEntry(Colours::red, errorMessage);
                uploadQuery->clear();
            } else {
                uint32 totalBlocks = miosStudio->uploadHandler->totalBlocks - miosStudio->uploadHandler->excludedBlocks;
                float timeUpload = miosStudio->uploadHandler->timeUpload;
                float transferRateKb = ((totalBlocks * 256) / timeUpload) / 1024;
                uploadStatus->addEntry(Colours::green, String::formatted(T("Upload of %d bytes completed after %3.2fs (%3.2f kb/s)"),
                                                                         totalBlocks*256,
                                                                         timeUpload,
                                                                         transferRateKb));

                if( miosStudio->uploadHandler->recoveredErrorsCounter > 0 ) {
                    uploadStatus->addEntry(Colours::grey, String::formatted(T("%d ignorable errors during upload solved (no issue!)"),
                                                                            miosStudio->uploadHandler->recoveredErrorsCounter));
                }

                uploadQuery->clear();
                uploadQuery->addEntry(Colours::grey, T("Waiting for reboot..."));
            }
            uploadStop();

            // delay must be long enough so that it is ensured that the application has booted and can repond on queries
            MultiTimer::startTimer(TIMER_DELAYED_PROGRESS_OFF, 5000);
        }
    } else if( timerId == TIMER_DELAYED_PROGRESS_OFF ) {
        if( !miosStudio->uploadHandler->busy() ) { // only if loader still unbusy
            progress = 0;
            queryCore(); // query for new application
        }
    } else if( timerId == TIMER_QUERY_CORE ) {
        if( miosStudio->uploadHandler->busy() ) {
            // wait again for 10 mS - the uploader provides an own timeout mechanism
            MultiTimer::startTimer(TIMER_QUERY_CORE, 10);
        } else {
            String errorMessage = miosStudio->uploadHandler->finish();
            uploadStop();

            if( errorMessage != String::empty ) {
                uploadQuery->clear();
                uploadQuery->addEntry(Colours::red, errorMessage);
                uploadQuery->addEntry(Colours::red, T("Check MIDI IN/OUT connections"));
                uploadQuery->addEntry(Colours::red, T("and Device ID!"));
            } else {
                String str;
                uploadQuery->clear();
                if( !(str=miosStudio->uploadHandler->coreOperatingSystem).isEmpty() )
                    uploadQuery->addEntry(Colours::black, "Operating System: " + str);
                if( !(str=miosStudio->uploadHandler->coreBoard).isEmpty() )
                    uploadQuery->addEntry(Colours::black, "Board: " + str);
                if( !(str=miosStudio->uploadHandler->coreFamily).isEmpty() )
                    uploadQuery->addEntry(Colours::black, "Core Family: " + str);
                if( !(str=miosStudio->uploadHandler->coreChipId).isEmpty() )
                    uploadQuery->addEntry(Colours::black, "Chip ID: 0x" + str);
                if( !(str=miosStudio->uploadHandler->coreSerialNumber).isEmpty() )
                    uploadQuery->addEntry(Colours::black, "Serial: #" + str);
                if( !(str=miosStudio->uploadHandler->coreFlashSize).isEmpty() )
                    uploadQuery->addEntry(Colours::black, "Flash Memory Size: " + str + " bytes");
                if( !(str=miosStudio->uploadHandler->coreRamSize).isEmpty() )
                    uploadQuery->addEntry(Colours::black, "RAM Size: " + str + " bytes");
                if( !(str=miosStudio->uploadHandler->coreAppHeader1).isEmpty() )
                    uploadQuery->addEntry(Colours::black, str);
                if( !(str=miosStudio->uploadHandler->coreAppHeader2).isEmpty() )
                    uploadQuery->addEntry(Colours::black, str);
            }
        }
    }
}
