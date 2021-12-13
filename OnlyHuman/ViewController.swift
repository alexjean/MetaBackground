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
    @IBOutlet var videoView: NSImageView!
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
        let img0 = NSImage(size: NSSize(width: 1280, height: 720))
        var img1: NSImage? = nil
        switch(backgroundSelector.stringValue) {
        case "LakeView":  img1 = NSImage(imageLiteralResourceName: "LakeView");                    break
        case "DimBar":    img1 = NSImage(imageLiteralResourceName: "DimBar");                      break
        case "Green":     img0.fillWith(color: NSColor(red: 0.6, green: 1, blue: 0.47, alpha: 1)); break
        case "Black":     img0.fillWith(color: NSColor.black);                                     break
        case "YouSelect": img1 = chooseImageFile();
                          if img1 == nil { return }
                          break
        default:          img0.fillWith(color: NSColor.white);                                     break
        }
        self.alexOutput = nil
        guard let img = img1 else {
            backgroundBuffer = img0.pixelBuffer()
            videoView.image = img0
            return
        }
        backgroundBuffer = img.pixelBuffer()?.resizeCropTo(width: 1280, height: 720)
        videoView.image = img
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
        setupVideoConstraints()
        videoView.translatesAutoresizingMaskIntoConstraints = false
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
        view.window?.level = .floating
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
    
    private func debugDump(child:NSView, tag:String) {
//        guard let parent = child.superview else {
//            print(String(format:"%s -- childView %.0f %.0f y=%.0f", tag
//                         , child.bounds.height, child.frame.height, child.bounds.origin.y))
//            return
//        }
//        let contraints = child.constraints
//        for contraint in contraints {
//            print(tag,contraint.debugDescription)
//        }
//        print(String(format:"%@ super  %.0f  %0.f y=%.0f -- childView %.0f %.0f y=%.0f", tag
//                     , parent.bounds.height, parent.frame.height, parent.bounds.origin.y
//                     , child.bounds.height, child.frame.height, child.bounds.origin.y))
//
    }
    
    // setup constraint by panelBox.isHide
    // the topAnchor's priority of VideoView in storybord is set to 900
    private var constraintToPanelBoxBottom: NSLayoutConstraint?
    private var constraintToViewTop:NSLayoutConstraint?
    private func setupVideoConstraints() {
        if constraintToPanelBoxBottom == nil {
            constraintToPanelBoxBottom = videoView.topAnchor.constraint(equalTo: panelBox.bottomAnchor)
        }
        if constraintToViewTop == nil {
            constraintToViewTop = videoView.topAnchor.constraint(equalTo: view.topAnchor)
        }
        videoView.translatesAutoresizingMaskIntoConstraints = false
        // put .isActive = false first to prevent annoying Layout warning
        if panelBox.isHidden {
            constraintToPanelBoxBottom?.isActive = false
            constraintToViewTop?.isActive = true
        } else {
            constraintToViewTop?.isActive = false
            constraintToPanelBoxBottom?.isActive = true
        }
        
    }
    
    public func panel(hide:Bool) {
        if panelBox.isHidden == hide { return }
        guard let win = view.window else {
            printMessageTime(msg: "Odd thing, no view.window")
            return
        }
        panelBox.isHidden = hide
        setupVideoConstraints()
        let sizeWin:CGSize
        var width0 = win.frame.width, height0 = win.frame.height
        let offset = panelBox.frame.height
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
            return
        }
        sizeWin = CGSize(width: width0, height: imgHeight + offset * 1)
        let winFrame = NSRect(origin: win.frame.origin, size: sizeWin)
        win.setFrame(winFrame, display: true)  // 當autoresizeMask on時，這條會改 minY
        //let constraint = NSAutoresizingMaskLayoutConstraint  // autoresizingMaskYAxisAnchor
        //view.setBoundsSize(sizeWin)    // Bounds一設，subview constraints全失效
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
    var debugCounter = 0
    var debugTotal = 0
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
                self.videoView.image = NSImage(cgImage: cgImage, size: NSSize(width: cgImage.width, height: cgImage.height))
                let duration =  timeMark.timeIntervalSinceNow
                self.msgLabel.stringValue = String(format: "%.0f%% dropped    %.0fms", Float(self.debugCounter)/Float(self.debugTotal)*100,-duration*1000)
            }
            self.debugRunning = false
        }
    }
    
    func chooseImageFile() -> NSImage? {
        let dialog = NSOpenPanel();
        dialog.title                   = "Choose a image file";
        dialog.showsResizeIndicator    = true;
        dialog.showsHiddenFiles        = false;
        dialog.canChooseDirectories    = true;
        dialog.canCreateDirectories    = true;
        dialog.allowsMultipleSelection = false;
        dialog.allowedContentTypes     = [.image]

        if (dialog.runModal() == NSApplication.ModalResponse.OK) {
            guard let url = dialog.url else { return nil }
            printMessageTime(msg: String(format: "Selected url: %@", url.debugDescription))
            let img = NSImage(contentsOf: url)
            if (img == nil) { print("Something wrong!") }
            return img
        } else {  // User clicked on "Cancel"
            return nil
        }
    }

}

