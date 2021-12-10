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

    fileprivate var trackingArea: NSTrackingArea?

    
    @IBAction func onClickBackgroundSelector(_ sender: Any) {
        let img: NSImage
        switch(backgroundSelector.stringValue) {
            case "LakeView":img = NSImage(imageLiteralResourceName: "LakeView");   break
            case "DimBar":  img = NSImage(imageLiteralResourceName: "DimBar");     break
            case "Green":   img = NSImage(size: NSSize(width: 1280, height: 720))
                            img.fillWith(color: NSColor(red: 0.6, green: 1, blue: 0.47, alpha: 1))
                            break
            //case "Transparency":                    break
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
    
    override func viewDidLoad() {
        super.viewDidLoad()
        backgroundSelector.stringValue = "LakeView"
        onClickBackgroundSelector([])
        loadCounter+=1; print("viewDidLoad \(loadCounter) times")
        
        //trackingArea = NSTrackingArea(rect: view.bounds, options: [.activeInActiveApp, .mouseEnteredAndExited], owner: view, userInfo: nil)
        //view.addTrackingArea(trackingArea!)
        
        let devices = Capturer.DiscoveryDevices()
        deviceSelector.removeAllItems()
        if devices.isEmpty {  deviceSelector.addItem(withTitle: "No video device!")  }
        for device in devices {  deviceSelector.addItem(withTitle: device.localizedName) }
        if captureSession == nil {
            captureSession = Capturer.Setup(device: devices.first, preview: onlyHumanView, controller: self)
        }
        guard let session = captureSession else { return }
        if !session.isRunning  { session.startRunning() }
    }
    
    override var representedObject: Any? {
        didSet {
        }
    }
    
    public func panel(show:Bool) {
        if panelBox.isHidden == (!show) { return }
        let b0 = onlyHumanView.bounds, size0:CGSize
        if show { size0 = CGSize(width: b0.width, height: b0.height + panelBox.bounds.height + 4)  }
        else    { size0 = CGSize(width: b0.width, height: b0.height)                          }
        panelBox.isHidden = show ? false: true
        //view.setFrameSize(size0)
    }
    /*
    private func setWindowBar(show:Bool) {
        guard let win = self.view.window else { return }
        let hasBar = win.hasTitleBar
        print("mouseOver show \(show) bar \(hasBar)")
        if  hasBar {
            if show { return }
            win.styleMask.remove(.titled)
        } else {
            if win.isKeyWindow { win.styleMask.update(with: .titled) }
            else if show { win.styleMask.update(with: .titled) }
        }
    }
    
    
    override func mouseEntered(with event: NSEvent) {
        // mouseEntered will fired when active comboBox list then select
        // so unusable , change to appDelegate 
        super.mouseEntered(with: event)
        setWindowBar(show:true)
    }
    
    override func mouseExited(with event: NSEvent) {
        super.mouseExited(with: event)
        setWindowBar(show:false)
    }
    */

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
            print("Debug blocking dropframe \(self.debugCounter)");
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

