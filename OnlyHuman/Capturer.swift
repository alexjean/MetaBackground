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


    static func Setup(device: AVCaptureDevice?, preview imageView:NSImageView?, controller:AVCaptureVideoDataOutputSampleBufferDelegate)-> AVCaptureSession? {
        guard let captureDevice = device else {
            print("No CaptureDevice.default")
            return nil
        }
        let captureSession = AVCaptureSession()
        captureSession.sessionPreset = .low
        guard let input = try? AVCaptureDeviceInput(device:captureDevice) else {
            print("Input error!")
            return nil
        }
        captureSession.addInput(input)
        captureSession.startRunning()

        if let view0 = imageView {
            let previewLayer = AVCaptureVideoPreviewLayer(session: captureSession)
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
        let dataOutput = AVCaptureVideoDataOutput()
        dataOutput.setSampleBufferDelegate(controller, queue: DispatchQueue(label: "videoQueue"))
        dataOutput.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: Int(kCVPixelFormatType_32BGRA)]
        captureSession.addOutput(dataOutput)
        return captureSession
    }

}
