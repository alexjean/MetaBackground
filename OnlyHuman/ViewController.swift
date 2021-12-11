//
//  ViewController.swift
//  OnlyHuman
//
//  Created by Alex on 2021/12/9.
//

import Cocoa
import CoreML
import AVKit
import Vision

class ViewController: NSViewController, AVCaptureVideoDataOutputSampleBufferDelegate {

    @IBOutlet var msgLabel: NSTextField!
    @IBOutlet var onlyHumanView: NSImageView!
    @IBOutlet var deviceSelector: NSPopUpButton!
    @IBOutlet var backgroundSelector: NSComboBox!
    @IBOutlet var panelBox: NSBox!
    
    private var alexOutput: alexmodelOutput?
    private var backgroundBuffer: CVPixelBuffer?
    private var m_mlConfig:MLModelConfiguration
    private var m_model:alexmodel?
   
    private var captureSession:AVCaptureSession?
    private var captureDevice:AVCaptureDevice?
    
    @IBAction func onClickBackgroundSelector(_ sender: Any) {
        let img: NSImage
        switch(backgroundSelector.stringValue) {
            case "LakeView":img = NSImage(imageLiteralResourceName: "LakeView");   break
            case "DimBar":  img = NSImage(imageLiteralResourceName: "DimBar");     break
            case "Green":   img = NSImage(size: NSSize(width: 1280, height: 720))
                            img.fillWith(color: NSColor(red: 0.6, green: 1, blue: 0.47, alpha: 1))
                            break
            case "Transparency":
                            img = NSImage(size: NSSize(width: 1280, height: 720))
                            img.fillWith(color: NSColor.black)
                            print(String(format:"view  %.0f  %0.f y=%.0f -- humanView %.0f %.0f y=%.0f"
                                         , view.bounds.height, view.frame.height, view.frame.origin.y
                                         , onlyHumanView.bounds.height, onlyHumanView.frame.height, onlyHumanView.frame.origin.y))
                            break
            default:        img = NSImage(size: NSSize(width: 1280, height: 720))
                            img.fillWith(color: NSColor.white)
                            break
        }
        backgroundBuffer = img.pixelBuffer()?.resizeCropTo(width: 1280, height: 720)
        onlyHumanView.image = img
        self.alexOutput = nil
    }
    
    private var loadCounter:Int = 0
    required init?(coder: NSCoder) {
        m_mlConfig=MLModelConfiguration()
        m_mlConfig.allowLowPrecisionAccumulationOnGPU = true
        m_mlConfig.computeUnits = .cpuAndGPU
        m_model = try? alexmodel(configuration: m_mlConfig)
        super.init(coder: coder)
    }
    
    private func prepareDeviceSelector()->[AVCaptureDevice] {
        let devices = Capturer.DiscoveryDevices()
        deviceSelector.removeAllItems()
        if devices.isEmpty {  deviceSelector.addItem(withTitle: "No video device!")  }
        for device in devices {  deviceSelector.addItem(withTitle: device.localizedName) }
        return devices
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        backgroundSelector.stringValue = "LakeView"
        onClickBackgroundSelector([])
        loadCounter+=1;
        printMessageTime(msg: "---viewDidLoad \(loadCounter) times")
        
        let devices = prepareDeviceSelector()
        captureDevice = devices.first
        if captureSession == nil {
            captureSession = Capturer.setupSession(dataDelegate: self)
        }
    }
    
    override var representedObject: Any? {
        didSet {
        }
    }
    
    func printMessageTime(msg:String) {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss"
        print(msg + " " + formatter.string(from: Date.now))
    }
    
    override func viewDidAppear() {
        if let session = captureSession {
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
    
    override func viewWillDisappear() {
        if let session = captureSession {
            if session.isRunning  {
                Capturer.removeInput(captureSession: session) // stopRunning in removeInput
                printMessageTime(msg: "===stopCapture")
            }
        }
    }
    
    public func panel(hide:Bool) {
        if panelBox.isHidden == hide { return }
        guard let win = view.window else {
            printMessageTime(msg: "Odd thing, no view.window")
            return
        }
        panelBox.isHidden = hide
        let sizeWin:CGSize
        var width0 = win.frame.width, height0 = win.frame.height
        let offset = panelBox.frame.height+2
        var imgHeight = hide ? (height0-offset) : height0
        if (hide) {
            let width1 = imgHeight * 16 / 9
            let height1 = width0 * 9 / 16
            if width0 > (width1 + 0.001) {
                width0 = width1
            } else if imgHeight > (height1 + 0.001) {
                imgHeight = height1
            }
            sizeWin = CGSize(width: width0, height: imgHeight)
            let winFrame = NSRect(origin: win.frame.origin, size: sizeWin)
            win.setFrame(winFrame, display: true)
            onlyHumanView.setFrameOrigin(NSPoint.zero)
            onlyHumanView.setFrameSize(NSSize(width:width0, height: imgHeight))
            print(String(format:"view  %.0f  %0.f y=%.0f -- humanView %.0f %.0f y=%.0f"
                         , view.bounds.height, view.frame.height, view.bounds.origin.y
                         , onlyHumanView.bounds.height, onlyHumanView.frame.height, onlyHumanView.bounds.origin.y))
           return
        }
        sizeWin = CGSize(width: width0, height: imgHeight + offset)
        let winFrame = NSRect(origin: win.frame.origin, size: sizeWin)
        //view.autoresizingMask = NSView.AutoresizingMask.none
        //view.translatesAutoresizingMaskIntoConstraints = true
        win.setFrame(winFrame, display: true)
        //view.setBoundsSize(size0)    // Bounds一設，subview constraints全失效
        //onlyHumanView.setFrameOrigin(NSPoint.zero)
        //onlyHumanView.setFrameSize(NSSize(width:width0, height: imgHeight))
        //panelBox.setFrameOrigin(NSPoint(x:0 , y:imgHeight))
    }
    
    func captureOutput(_ output: AVCaptureOutput, didDrop sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        print("failed", Date())
    }
    
    func captureOutput(_ output: AVCaptureOutput, didOutput sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        guard let pixelBuffer: CVPixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }
        guard let buf = backgroundBuffer else { return }
        doAlexMLHandler(mlConfig: self.m_mlConfig, src: pixelBuffer, background: buf)
    }
    
    var debugRunning: Bool = false
    var debugCounter = 0, debugTotal = 0
    func doAlexMLHandler(mlConfig:MLModelConfiguration, src pixelBuffer:CVPixelBuffer, background bgBuffer:CVPixelBuffer) {
        self.debugTotal+=1
        if self.debugRunning {
            // print("Debug blocking dropframe \(self.debugCounter)");
            self.debugCounter+=1
            return
        }
        else  { self.debugRunning = true }
        let timeMark = Date.now
        DispatchQueue.global(qos: .userInteractive).async  {
            guard let copy = pixelBuffer.resizeCropTo(width: 1280, height: 720) else { return }
            guard let model = self.m_model else { return }
            let out0 = self.alexOutput
            let r1 = out0?.r1o, r2 = out0?.r2o
            let r3 = out0?.r3o, r4 = out0?.r4o
            let alexInput = alexmodelInput(src: copy, bgd: bgBuffer, r1i: r1, r2i: r2, r3i: r3, r4i: r4)
            do {
                let out1 = try model.prediction(input: alexInput)
                self.alexOutput = out1
            } catch { fatalError("model.prediction error!\r\n ==>"+error.localizedDescription) }  // Failed to src yuv2
            
            guard let buf = self.alexOutput?.fgr else { return }
            let image = CIImage(cvPixelBuffer: buf, options: [:])
            let cgImage = CIContext(options: nil).createCGImage(image, from: image.extent)!
            DispatchQueue.main.async {
                self.onlyHumanView.image = NSImage(cgImage: cgImage, size: NSSize(width: cgImage.width, height: cgImage.height))
                let duration =  timeMark.timeIntervalSinceNow
                self.msgLabel.stringValue = String(format: "%.0f%% dropped    %.0fms", Float(self.debugCounter)/Float(self.debugTotal)*100,-duration*1000)
            }
            self.debugRunning = false
        }
    }
    
    

}

