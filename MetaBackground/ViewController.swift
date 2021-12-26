//
//  ViewController.swift
//  OnlyHuman
//
//  Created by Alex on 2021/12/9.
//

import Cocoa
import AVKit

extension ViewController: ImageMessageDelegate {
    func setImageMessage(image: CGImage, message: String) {
        DispatchQueue.main.async {
            self.videoView.image = NSImage(cgImage: image, size: NSSize(width: image.width, height: image.height))
            self.msgLabel.stringValue = message
        }
    }
}

class ViewController: NSViewController{

    @IBOutlet var msgLabel: NSTextField!
    @IBOutlet var videoView: NSImageView!
    @IBOutlet var deviceSelector: NSPopUpButton!
    @IBOutlet var backgroundSelector: NSComboBox!
    @IBOutlet var panelBox: NSBox!
    
    let m_dataProcess = DataProcess()
    let m_capturer = Capturer()
    
    var m_device:AVCaptureDevice?
    var m_deviceList:[AVCaptureDevice] = []

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        m_dataProcess.myDelegate = self
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
        guard let img = img1 else {
            m_dataProcess.setBackgroundBuffer(buffer: img0.pixelBuffer())
            videoView.image = img0
            return
        }
        m_dataProcess.setBackgroundBuffer(buffer: img.pixelBuffer()?.resizeCropTo(width: 1280, height: 720))
        videoView.image = img
    }
    
    @IBAction func onClickDeviceSelector(_ sender: Any) {
        guard let button = sender as? NSPopUpButton else {
            return
        }
        let index = button.indexOfSelectedItem
        print("device \(index) \(button.itemTitle(at: index)) selected!")
        let count = m_deviceList.count
        if count <= index {
            print("Strange \(count) devices , we got index \(index) !")
            return
        }
        let device = m_deviceList[index]
        if !device.isConnected { return }
        m_device = device
        m_capturer.stop()
        m_capturer.start(device: device)
    }
    
    private func buildDeviceSelector(devices:[AVCaptureDevice]) {
        m_deviceList = devices
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
        if (devices.count != m_deviceList.count) {
            buildDeviceSelector(devices: devices)
            return
        }
        for device in devices {
            if m_deviceList.contains(device) { continue }
            buildDeviceSelector(devices: devices)
            return
        }
    }
    
    private var loadCounter:Int = 0
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
        m_device = devices.first
        m_deviceList = devices
        m_capturer.setupSession(dataDelegate: m_dataProcess)    // setup only once
        NotificationCenter.default.addObserver(self, selector: #selector(dropdownMenuOpened),
                                               name: NSPopUpButton.willPopUpNotification,
                                               object: nil)
    }
       
    override func viewDidAppear() {
        view.window?.level = .floating
        m_capturer.start( device: m_device)
    }
    
    override func viewWillDisappear() {
        m_capturer.stop()
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
        win.hasShadow = !hide
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
        sizeWin = CGSize(width: width0, height: imgHeight + offset)
        let winFrame = NSRect(origin: win.frame.origin, size: sizeWin)
        win.setFrame(winFrame, display: true)  // 當autoresizeMask on時，這條會改 minY
        //let constraint = NSAutoresizingMaskLayoutConstraint  // autoresizingMaskYAxisAnchor
        //view.setBoundsSize(sizeWin)    // Bounds一設，subview constraints全失效
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
//        panel(hide: true)
        
        guard let win = view.window else { return nil }
        let windowID = CGWindowID(win.windowNumber)
        if windowID == CGWindowID.zero {
            print("Window not found!")
            return nil
        }
//        guard let img = Ut.shared.captureWindowFromScreen(winId:windowID, rect:RECT.zero) else { return nil }

        var client = Ut.shared.getWindowRect(winId: windowID)
        //client.left -= 1
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

extension ViewController {
    func printMessageTime(msg:String) {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss"
        print(msg + " " + formatter.string(from: Date.now))
    }
}

