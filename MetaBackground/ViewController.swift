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
   
    private var m_captureSession:AVCaptureSession?
    private var m_captureDevice:AVCaptureDevice?
    private var m_captureDevices:[AVCaptureDevice] = []
    
    private var loadCounter:Int = 0
    required init?(coder: NSCoder) {
        m_mlConfig=MLModelConfiguration()
        m_mlConfig.allowLowPrecisionAccumulationOnGPU = true
        m_mlConfig.computeUnits = .cpuAndGPU
        m_model = try? alexmodel(configuration: m_mlConfig)
        super.init(coder: coder)
    }
    
    @IBAction func onClickBackgroundSelector(_ sender: Any) {
        let img0 = NSImage(size: NSSize(width: 1280, height: 720))
        var img1: NSImage? = nil
        switch(backgroundSelector.stringValue) {
        case "Transparent": img1 = rollingBackground();                                            break
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
    
    @IBAction func onClickDeviceSelector(_ sender: Any) {
        guard let button = sender as? NSPopUpButton else {
            return
        }
        let index = button.indexOfSelectedItem
        print("device \(index) \(button.itemTitle(at: index)) selected!")
        let count = m_captureDevices.count
        if count <= index {
            print("Strange \(count) devices , we got index \(index) !")
            return
        }
        let device = m_captureDevices[index]
        if !device.isConnected { return }
        m_captureDevice = device
        Capturer.stopCapture(session: m_captureSession)
        Capturer.startCapture(session: m_captureSession, device: device)
    }
    
    private func buildDeviceSelector(devices:[AVCaptureDevice]) {
        deviceSelector.removeAllItems()
        if devices.isEmpty {
            deviceSelector.addItem(withTitle: "No video device!")
            return
        }
        for device in devices {
            deviceSelector.addItem(withTitle: device.localizedName)
        }
    }
    
    @objc func dropdownMenuOpened() {
        let devices = Capturer.DiscoveryDevices()
        if (devices.count != m_captureDevices.count) {
            m_captureDevices = devices
            buildDeviceSelector(devices: devices)
            return
        }
        for device in devices {
            if m_captureDevices.contains(device) { continue }
            m_captureDevices = devices
            buildDeviceSelector(devices: devices)
            return
        }
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        backgroundSelector.stringValue = "LakeView"
        onClickBackgroundSelector([])
        setupVideoConstraints()
        videoView.translatesAutoresizingMaskIntoConstraints = false
        loadCounter+=1;
        printMessageTime(msg: "---viewDidLoad \(loadCounter) times")
        
        let devices = Capturer.DiscoveryDevices()
        buildDeviceSelector(devices: devices)
        m_captureDevice = devices.first
        m_captureDevices = devices
        if m_captureSession == nil {
            m_captureSession = Capturer.setupSession(dataDelegate: self)
        }
        
        NotificationCenter.default.addObserver(self, selector: #selector(dropdownMenuOpened),
                                               name: NSPopUpButton.willPopUpNotification,
                                               object: nil)

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
        Capturer.startCapture(session:m_captureSession, device: m_captureDevice)
    }
    
    override func viewWillDisappear() {
        Capturer.stopCapture(session:m_captureSession)
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
    
    var m_isRunning: Bool = false
    var debugCounter = 0, debugTotal = 0
    func doAlexMLHandler(mlConfig:MLModelConfiguration, src pixelBuffer:CVPixelBuffer, background bgBuffer:CVPixelBuffer) {
        self.debugTotal+=1
        if self.m_isRunning { self.debugCounter+=1; return }
        else                { self.m_isRunning = true }
        
        let timeMark = Date.now
        DispatchQueue.global(qos: .userInteractive).async  {
            // <A> resizeCropTo cost 10ms
            defer {  self.m_isRunning = false }
            guard let copy = pixelBuffer.resizeCropTo(width: 1280, height: 720) else { return }
            guard let model = self.m_model else { return }
            // <B> model cost 46ms
            let out0 = self.alexOutput
            let r1 = out0?.r1o, r2 = out0?.r2o
            let r3 = out0?.r3o, r4 = out0?.r4o
            let alexInput = alexmodelInput(src: copy, bgd: bgBuffer, r1i: r1, r2i: r2, r3i: r3, r4i: r4)
            do {
                let out1 = try model.prediction(input: alexInput)
                self.alexOutput = out1
            } catch { fatalError("model.prediction error!\r\n ==>"+error.localizedDescription) }  // Failed to src yuv2

            guard let buf = self.alexOutput?.fgr else { return }
            // <C> buf to CGImage two line cost 20ms
            // <A>+<C> cost 26ms , <A>+<B>+<C> cost 65ms
            let image = CIImage(cvPixelBuffer: buf, options: [:])
            let cgImage = CIContext(options: nil).createCGImage(image, from: image.extent)!

            DispatchQueue.main.async {
                self.videoView.image = NSImage(cgImage: cgImage, size: NSSize(width: cgImage.width, height: cgImage.height))
                let duration =  timeMark.timeIntervalSinceNow
                self.msgLabel.stringValue = String(format: "%.0f%% dropped    %.0fms", Float(self.debugCounter)/Float(self.debugTotal)*100,-duration*1000)
            }
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
    
    func rollingBackground() -> NSImage? {
//        let windowID = Ut.shared.getWindowID(bundleID: "be.goodbrain.MetaBackground", winTitle: "MetaBackground")
//        let windowID = Ut.shared.getWindowID(bundleID: "com.blizzard.worldofwarcraft", winTitle: "魔兽世界")

        guard let win = view.window else { return nil }
        let windowID = CGWindowID(win.windowNumber)
        if windowID == CGWindowID.zero {
            print("Window not found!")
            return nil
        }
//        guard let img = Ut.shared.captureWindowFromScreen(winId:windowID, rect:RECT.zero) else { return nil }

        var client = Ut.shared.getWindowRect(winId: windowID)
        client.top += panelBox.bounds.height         // CGRect 左上是0， 扣掉Panel高
        let cgRect  = client.toCGRect()
//        var nsRect = win.frame
//        nsRect.size.height -= panelBox.bounds.height
//        let cgRect = NSRectToCGRect(nsRect)
        guard let img = CGWindowListCreateImage(cgRect, [.optionOnScreenBelowWindow], windowID, [.bestResolution])
        else { return nil }
        let nsImage = NSImage(cgImage: img, size: CGSize(width:1280, height:720))
        return nsImage
//        backgroundBuffer = nsImage.pixelBuffer()?.resizeCropTo(width: 1280, height: 720)
//        videoView.image = nsImage
    }

}

