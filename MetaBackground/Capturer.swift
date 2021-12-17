//
//  Capturer.swift
//  OnlyHuman
//
//  Created by Alex on 2021/12/9.
//
import Vision
import AVKit

class Capturer: NSObject {
    static func DiscoveryDevices() -> [AVCaptureDevice] {
        let discoverySession = AVCaptureDevice.DiscoverySession(
            deviceTypes:[.builtInWideAngleCamera, .externalUnknown],
            mediaType: .video , position: .unspecified)
        return discoverySession.devices
    }

    static func addInput(device:AVCaptureDevice, session:AVCaptureSession) {
        do {
            let input = try AVCaptureDeviceInput(device:device)
            session.addInput(input)
        } catch {  print("Capture error:"+error.localizedDescription) }
    }

    static func linkPreview(imageView:NSImageView?, session: AVCaptureSession) {
        if let view0 = imageView {
            let previewLayer = AVCaptureVideoPreviewLayer(session: session)
            let conn = previewLayer.connection
            if conn?.isVideoMirroringSupported == true {
//                    conn.automaticallyAdjustsVideoMirroring = false
//                    conn.isVideoMirrored = true
              }
            view0.wantsLayer = false   // wantsLayer = false to let preview not show
            view0.layer?.addSublayer(previewLayer)
            let frame1 = view0.frame
            previewLayer.frame = NSRect(x:0, y:0, width: frame1.width, height:frame1.height)
        }
    }
    
    static func prepareOutput(delegate:AVCaptureVideoDataOutputSampleBufferDelegate, session:AVCaptureSession) {
        let dataOutput = AVCaptureVideoDataOutput()
        dataOutput.setSampleBufferDelegate(delegate, queue: DispatchQueue(label: "videoQueue"))
        dataOutput.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: Int(kCVPixelFormatType_32BGRA)]
        dataOutput.alwaysDiscardsLateVideoFrames = true
        if session.canAddOutput(dataOutput) {
            session.addOutput(dataOutput)
        } else { print("The capture session cannot Add Format_32BGRA") }
    }
    
    static func setupSession(dataDelegate :AVCaptureVideoDataOutputSampleBufferDelegate)-> AVCaptureSession? {
        let session = AVCaptureSession()
        session.sessionPreset = .low
       
        prepareOutput(delegate: dataDelegate, session: session)
        session.commitConfiguration()
        return session
    }
    
    static func removeInput(captureSession: AVCaptureSession) {
        if captureSession.isRunning { captureSession.stopRunning() }
        if let inputs = captureSession.inputs as? [AVCaptureDeviceInput] {
            for input in inputs {
                captureSession.removeInput(input)
            }
        }
    }
    
    static func startCapture(session :AVCaptureSession?, device captureDevice:AVCaptureDevice?) {
        if let session = session {
            if !session.isRunning  {
                if session.inputs.isEmpty {
                    guard let device = captureDevice else {
                        printMessageTime(msg: "...No device to startCapture")
                        return
                    }
                    Capturer.addInput(device: device, session: session)
                }
                session.startRunning()
                printMessageTime(msg: "...startCapture")
            }
        }
    }
    
    static func stopCapture(session: AVCaptureSession?) {
        if let session = session {
            if session.isRunning  {
                Capturer.removeInput(captureSession: session) // stopRunning in removeInput
                printMessageTime(msg: "===stopCapture")
            }
        }
    }
    
    static func printMessageTime(msg:String) {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss"
        print(msg + " " + formatter.string(from: Date.now))
    }



}
