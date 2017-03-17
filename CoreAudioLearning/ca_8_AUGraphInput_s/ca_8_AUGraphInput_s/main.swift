//
//  main.swift
//  ca_8_AUGraphInput_s
//
//  Created by Timothy Shull on 7/7/16.
//  Copyright (c) 2016 tim_shull. All rights reserved.
//

import AudioToolbox
import CoreAudio
import AudioUnit

//extension Bool {
//    init<T:IntegerType>(_ integer: T) {
//        if integer == 0 {
//            self.init(false)
//        } else {
//            self.init(true)
//        }
//    }
//}

func intToBool<T:IntegerType>(intVal: T) -> Bool {
    if intVal == 0 {
        return false
    } else {
        return true
    }
}

struct CustomAUGraphPlayer {
    var streamFormat: AudioStreamBasicDescription
    var graph: AUGraph
    var inputUnit: AudioUnit
    var outputUnit: AudioUnit
    var inputBuffer: UnsafeMutablePointer<AudioBufferList>
//AudioBufferList *inputBuffer;
    var ringBuffer: UnsafeMutablePointer<CARingBuffer<Double>>
//CARingBuffer *ringBuffer;
    var firstInputSampleTime: Float64
    var firstOutputSampleTime: Float64
    var inToOutSampleTimeOffset: Float64
}

func checkError(error: OSStatus, operation: String) -> Void {
    if error == noErr {
        return
    }

    fputs("Error: \(operation) (\(error))", stderr)
    exit(1)
}

func createInputUnit(inout player: CustomAUGraphPlayer) -> Void {
    var inputCD: AudioComponentDescription = AudioComponentDescription()
    inputCD.componentType = kAudioUnitType_Output
    inputCD.componentSubType = kAudioUnitSubType_HALOutput
    inputCD.componentManufacturer = kAudioUnitManufacturer_Apple

    var comp: AudioComponent = AudioComponentFindNext(nil, &inputCD)
    if comp == nil {
        print("Unable to create output unit")
        exit(-1)
    }

    checkError(AudioComponentInstanceNew(comp, &player.inputUnit), operation: "Couldn't open component for inputUnit")

    var disableFlag: UInt32 = 0
    var enableFlag: UInt32 = 1
    var outputBus: AudioUnitScope = 0
    var inputBus: AudioUnitScope = 1

    checkError(AudioUnitSetProperty(player.inputUnit,
            kAudioOutputUnitProperty_EnableIO,
            kAudioUnitScope_Input,
            inputBus,
            &enableFlag,
            UInt32(strideofValue(enableFlag))),
            operation: "Couldn't enable input on I/O unit")

    checkError(AudioUnitSetProperty(player.inputUnit,
            kAudioOutputUnitProperty_EnableIO,
            kAudioUnitScope_Output,
            outputBus,
            &disableFlag, // well crap, have to disable
            UInt32(strideofValue((enableFlag)))),
            operation: "Couldn't disable output on I/O unit")

    // set device (osx only... iphone has only one device)
    var defaultDevice: AudioDeviceID = kAudioObjectUnknown
    var propertySize: UInt32 = UInt32(strideofValue(defaultDevice))

    // AudioHardwareGetProperty() is deprecated
    //	CheckError (AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice,
    //										 &propertySize,
    //										 &defaultDevice),
    //				"Couldn't get default input device");

    // AudioObjectProperty stuff new in 10.6, replaces AudioHardwareGetProperty() call
    // TODO: need to update ch08 to explain, use this call. need CoreAudio.framework
    var defaultDeviceProperty: AudioObjectPropertyAddress = AudioObjectPropertyAddress()
    defaultDeviceProperty.mSelector = kAudioHardwarePropertyDefaultInputDevice
    defaultDeviceProperty.mScope = kAudioObjectPropertyScopeGlobal
    defaultDeviceProperty.mElement = kAudioObjectPropertyElementMaster

    checkError(AudioObjectGetPropertyData(UInt32(kAudioObjectSystemObject),
            &defaultDeviceProperty,
            0,
            nil,
            &propertySize,
            &defaultDevice),
            operation: "Couldn't get default input device")

    // set this defaultDevice as the input's property
    // kAudioUnitErr_InvalidPropertyValue if output is enabled on inputUnit
    checkError(AudioUnitSetProperty(player.inputUnit,
            kAudioOutputUnitProperty_CurrentDevice,
            kAudioUnitScope_Global,
            outputBus,
            &defaultDevice,
            UInt32(strideofValue(defaultDevice))),
            operation: "Couldn't set default device on I/O unit")

    // use the stream format coming out of the AUHAL (should be de-interleaved)
    propertySize = UInt32(strideof(AudioStreamBasicDescription))
    checkError(AudioUnitGetProperty(player.inputUnit,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output,
            inputBus,
            &player.streamFormat,
            &propertySize),
            operation: "Couldn't get ASBD from input unit");

    // 9/6/10 - check the input device's stream format
    var deviceFormat: AudioStreamBasicDescription;
    checkError(AudioUnitGetProperty(player.inputUnit,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            inputBus,
            &deviceFormat,
            &propertySize),
            operation: "Couldn't get ASBD from input unit")

    print("Device rate \(deviceFormat.mSampleRate), graph rate \(player.streamFormat.mSampleRate)\n")
    player.streamFormat.mSampleRate = deviceFormat.mSampleRate

    propertySize = UInt32(strideof(AudioStreamBasicDescription))
    checkError(AudioUnitSetProperty(player.inputUnit,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output,
            inputBus,
            &player.streamFormat,
            propertySize),
            operation: "Couldn't set ASBD on input unit")

    /* allocate some buffers to hold samples between input and output callbacks
     (this part largely copied from CAPlayThrough) */
    //Get the size of the IO buffer(s)
    var bufferSizeFrames: UInt32 = 0
    propertySize = UInt32(sizeof(UInt32))
    checkError(AudioUnitGetProperty(player.inputUnit,
            kAudioDevicePropertyBufferFrameSize,
            kAudioUnitScope_Global,
            0,
            &bufferSizeFrames,
            &propertySize),
            operation: "Couldn't get buffer frame size from input unit")
    var bufferSizeBytes: UInt32 = bufferSizeFrames * UInt32(strideof(Float32));

    if intToBool(player.streamFormat.mFormatFlags & kAudioFormatFlagIsNonInterleaved) {
        print("format is non-interleaved\n")
        // allocate an AudioBufferList plus enough space for array of AudioBuffers
        var propsize: Int = strideof(UInt32) + (strideof(AudioBuffer) * Int(player.streamFormat.mChannelsPerFrame))

        //malloc buffer lists
        player.inputBuffer = UnsafeMutablePointer.alloc(propsize)

        withUnsafeMutablePointer(&player.inputBuffer, {
            (ptr: UnsafePointer<AudioBufferList>) -> Void in
//            ptr.mNumberBuffers = player.streamFormat.mChannelsPerFrame
            player.inputBuffer.mNumberBuffers = player.streamFormat.mChannelsPerFrame
            for var i = 0; i < player.inputBuffer.mNumberBuffers; i += 1 {
                player.inputBuffer.mBuffers[i].mNumberChannels = 1
                player.inputBuffer.mBuffers[i].mDataByteSize = bufferSizeBytes
                player.inputBuffer.mBuffers[i].mData = UnsafeMutablePointer.alloc(bufferSizeBytes)
            }


        })


        //pre-malloc buffers for AudioBufferLists

    } else {
        print("format is interleaved\n");
        // allocate an AudioBufferList plus enough space for array of AudioBuffers
        var propsize: Int = strideof(UInt32) + (strideof(AudioBuffer) * 1)

        //malloc buffer lists
        player.inputBuffer = UnsafeMutablePointer.alloc(propsize)
//        withUnsafeMutablePointer(&player.inputBuffer, {
//            (ptr: UnsafePointer<AudioBufferList>) -> Void in
//            player.inputBuffer.mNumberBuffers = 1;
//
//            //pre-malloc buffers for AudioBufferLists
//            player.inputBuffer.mBuffers[0].mNumberChannels = player.streamFormat.mChannelsPerFrame
//            player.inputBuffer.mBuffers[0].mDataByteSize = bufferSizeBytes
//            player.inputBuffer.mBuffers[0].mData = UnsafeMutablePointer.alloc(bufferSizeBytes)
//        }
    }

    //Alloc ring buffer that will hold data between the two audio devices
    player.ringBuffer = UnsafeMutablePointer<CARingBuffer<Double>>.alloc(strideof(CARingBuffer<Double>))
    player.ringBuffer.initialize(CARingBuffer<Double>(numberOfChannels: player.streamFormat.mChannelsPerFrame, capacityFrames: player.streamFormat.mBytesPerFrame))

    // set render proc to supply samples from input unit
    var callbackStruct: AURenderCallbackStruct
    callbackStruct.inputProc = InputRenderProc;
    withUnsafeMutablePointer(&player, {
        callbackStruct.inputProcRefCon = player;
    })


    checkError(AudioUnitSetProperty(player.inputUnit,
            kAudioOutputUnitProperty_SetInputCallback,
            kAudioUnitScope_Global,
            0,
            &callbackStruct,
            UInt32(strideofValue(callbackStruct))),
            operation: "Couldn't set input callback");

    checkError(AudioUnitInitialize(player.inputUnit),
            operation: "Couldn't initialize input unit");

    player.firstInputSampleTime = -1
    player.inToOutSampleTimeOffset = -1

    print("Bottom of CreateInputUnit()\n")
}

var player: CustomAUGraphPlayer

createInputUnit(&player)
//createCustomAUGraph(&player)

