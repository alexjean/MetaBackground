//
//  VideoCapture.swift
//  MetaBackground
//
//  Created by Alex on 2021/12/26.
//

import Foundation
import AVKit
import Vision
import CoreML

protocol ImageMessageDelegate {
    func setImageMessage(image:CGImage, message:String)
}

class DataProcess: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate {

    var showDelegate: ImageMessageDelegate?
    
    private let mlConfig:MLModelConfiguration
    private let model:alexmodel?

    private var alexOutput: alexmodelOutput?
    private var isRunning: Bool = false
    private var blockData: Bool = false
    private var debugCounter = 0, debugTotal = 0
    private var backgroundBuffer:CVPixelBuffer?
    
    override init() {
        mlConfig=MLModelConfiguration()
        mlConfig.allowLowPrecisionAccumulationOnGPU = true
        mlConfig.computeUnits = .cpuAndGPU
        model = try? alexmodel(configuration: mlConfig)
        super.init()
    }

    func setBackgroundBuffer(buffer:CVPixelBuffer?) {
        backgroundBuffer = buffer
    }
    
    func setBlockData(_ block:Bool) {
        blockData = block
    }
    
    func isBlocking() -> Bool {
        return blockData
    }
    
    
    func captureOutput(_ output: AVCaptureOutput, didDrop sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        print("failed", Date())
    }
    
    func captureOutput(_ output: AVCaptureOutput, didOutput sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        if (blockData) { return }
        guard let pixelBuffer: CVPixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }
        guard let buf = backgroundBuffer else { return }
        doAlexMLHandler(mlConfig: mlConfig, src: pixelBuffer, background: buf)
    }
    
    func doAlexMLHandler(mlConfig:MLModelConfiguration, src pixelBuffer:CVPixelBuffer, background bgBuffer:CVPixelBuffer) {
        debugTotal+=1
        if isRunning { debugCounter+=1; return }
        else         { isRunning = true }
        
        let timeMark = Date.now
        DispatchQueue.global(qos: .userInteractive).async  {
            // <A> resizeCropTo cost 10ms
            defer {  self.isRunning = false }
            guard let copy = pixelBuffer.resizeCropTo(width: 1280, height: 720) else { return }
            guard let model = self.model else { return }
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
            let message = String(format: "%.0f%% dropped    %.0fms", Float(self.debugCounter)/Float(self.debugTotal)*100,
                                 -timeMark.timeIntervalSinceNow*1000)
            self.showDelegate?.setImageMessage(image: cgImage, message: message)
        }
    }
}
