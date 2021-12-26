//
//  Capturer.swift
//  OnlyHuman
//
//  Created by Alex on 2021/12/9.
//
import Vision
import AVKit

extension Capturer {
    static func DiscoveryDevices() -> [AVCaptureDevice] {
        let discoverySession = AVCaptureDevice.DiscoverySession(
            deviceTypes:[.builtInWideAngleCamera, .externalUnknown],
            mediaType: .video , position: .unspecified)
        return discoverySession.devices
    }

    func debug(msg:String, func1:String = #function) {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss"
        print(msg + "  on \(func1) " + formatter.string(from: Date.now))
    }
    
    func message(msg:String) {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss"
        print(msg + "  " + formatter.string(from: Date.now))
    }
}

class Capturer: NSObject {
    var session: AVCaptureSession? = nil

    func addInput(device:AVCaptureDevice) {
        guard let session = session else { return }
        do {
            let input = try AVCaptureDeviceInput(device:device)
            session.addInput(input)
        } catch {  debug(msg:"Capture error:"+error.localizedDescription) }
    }

    func linkPreview(imageView:NSImageView?) {
        guard let session = session else { return }
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
    
    func setupSession(dataDelegate :AVCaptureVideoDataOutputSampleBufferDelegate) {
        if session != nil { return }  // setup only once
        session = AVCaptureSession()
        guard let session = session else { return }
        session.sessionPreset = .low
        let dataOutput = AVCaptureVideoDataOutput()
        dataOutput.setSampleBufferDelegate(dataDelegate, queue: DispatchQueue(label: "videoQueue"))
        dataOutput.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: Int(kCVPixelFormatType_32BGRA)]
        dataOutput.alwaysDiscardsLateVideoFrames = true
        if session.canAddOutput(dataOutput) {
            session.addOutput(dataOutput)
        } else { debug(msg:"The capture session cannot Add Format_32BGRA") }
        session.commitConfiguration()
    }
    
    func removeInput() {
        guard let session = session else { return }
        if session.isRunning { session.stopRunning() }
        if let inputs = session.inputs as? [AVCaptureDeviceInput] {
            for input in inputs {
                session.removeInput(input)
            }
        }
    }
    
    func start(device captureDevice:AVCaptureDevice?) {
        guard let session = session else { return }
        if !session.isRunning  {
            if session.inputs.isEmpty {
                guard let device = captureDevice else {
                    debug(msg: "...No device to startCapture")
                    return
                }
                addInput(device: device)
            }
            session.startRunning()
            message(msg: "...startCapture")
        }
    }
    
    func stop() {
        guard let session = session else { return }
        if session.isRunning  {
            removeInput() // stopRunning in removeInput
            message(msg: "===stopCapture")
        }
    }
    
}
