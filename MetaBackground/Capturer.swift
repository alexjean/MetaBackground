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
    var previewLayer:AVCaptureVideoPreviewLayer? = nil

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
            if previewLayer == nil {
                previewLayer = AVCaptureVideoPreviewLayer(session: session)
            }
            guard let layer0 = previewLayer else { return }
            let conn = layer0.connection
            if conn?.isVideoMirroringSupported == true {
//                    conn.automaticallyAdjustsVideoMirroring = false
//                    conn.isVideoMirrored = true
            }
            view0.wantsLayer = true   // when wantsLayer = true , will create a backLayer
//            view0.layer?.sublayers?.removeAll()  // 因為不知何時 BackingLayer會加到Sublayer卡在前面
//            view0.layer?.addSublayer(layer0)
            view0.layer?.insertSublayer(layer0, at: 0)
            let frame1 = view0.frame
            layer0.frame = NSRect(x:0, y:0, width: frame1.width, height:frame1.height)
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
            session.stopRunning()
            removeInput() // stopRunning in removeInput
            message(msg: "===stopCapture")
        }
    }
    
}
