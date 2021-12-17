//
//  File.swift
//  Camera_macOS
//
//  Created by Alex on 2021/12/4.
//

import Vision
import AppKit

extension String: Error {}

public extension CVPixelBuffer {
    
    func resizeCropTo(width:Int, height:Int) -> CVPixelBuffer? {
        precondition(CFGetTypeID(self) == CVPixelBufferGetTypeID(), "resizeCropTo() cannot be called on a non-CVPixelBuffer")
        var _copy: CVPixelBuffer?
        let w0 = CVPixelBufferGetWidth(self)
        let h0 = CVPixelBufferGetHeight(self)
        if w0 == width && h0 == height { return self }
        let formatType = CVPixelBufferGetPixelFormatType(self)
        let attachments = CVBufferCopyAttachments(self, .shouldPropagate)

        CVPixelBufferCreate(nil, width, height, formatType, attachments, &_copy)
        guard let copy = _copy else {  return nil  }

        let image = CIImage(cvPixelBuffer: self, options: [:])
        let scaleX = CGFloat(width) / image.extent.width, scaleY = CGFloat(height) / image.extent.height
        let scale = max(scaleX, scaleY)
        let ciImage = image.transformed(by: CGAffineTransform(scaleX: scale, y: scale))
            .cropped(to: CGRect(x: 0, y: 0, width: width, height: height))
        let context = CIContext(options: [:])
        context.render(ciImage, to: copy)
        return copy
    }
    
    func toNSImage(width:Int, height:Int) -> NSImage? {
        precondition(CFGetTypeID(self) == CVPixelBufferGetTypeID(), "toNSImage() cannot be called on a non-CVPixelBuffer")
        let ciImage = CIImage(cvPixelBuffer: self, options: [:])
        let cgImage = CIContext(options: nil).createCGImage(ciImage, from: ciImage.extent)!
        return NSImage(cgImage: cgImage, size: NSSize(width: width, height: height))
    }
    
    func replaceAlpha(alpha alphaBuffer:CVPixelBuffer, background bgColor:NSColor) throws -> CVPixelBuffer {
        precondition(CFGetTypeID(self) == CVPixelBufferGetTypeID(), "replaceAlpha() cannot be called on a non-CVPixelBuffer")

        var _copy: CVPixelBuffer?
        let width = CVPixelBufferGetWidth(self)
        let height = CVPixelBufferGetHeight(self)
        let formatType = CVPixelBufferGetPixelFormatType(self)
        let attachments = CVBufferCopyAttachments(self, .shouldPropagate)

        CVPixelBufferCreate(nil, width, height, formatType, attachments, &_copy)
        guard let copy = _copy else {  throw "CVPixelBuffer.replacAlphaFailed"  }

        guard (width == CVPixelBufferGetWidth(alphaBuffer))   else { return self }
        guard (height == CVPixelBufferGetHeight(alphaBuffer)) else { return self }
        guard kCVPixelFormatType_OneComponent8 == CVPixelBufferGetPixelFormatType(alphaBuffer)
        else { throw String(format: "alphaBuffer formatType isn't OneComponent8") }
        
        let offset:Int
        if (formatType == kCVPixelFormatType_32BGRA || formatType == kCVPixelFormatType_32RGBA)
             { offset = 3 }        // BGRA
        else if (formatType == kCVPixelFormatType_32ABGR || formatType == kCVPixelFormatType_32ARGB)
             { offset = 0 }       // ARGB
        else { throw String(format: "CVPixelBuffer.UnknowFormatType %x", formatType) }

        guard CVPixelBufferGetPlaneCount(self) == 0 else { return self }
        
        CVPixelBufferLockBaseAddress(self, .readOnly)
        CVPixelBufferLockBaseAddress(copy, [])
//        CVPixelBufferLockBaseAddress(self, [])
        CVPixelBufferLockBaseAddress(alphaBuffer, .readOnly)
        defer {
            CVPixelBufferUnlockBaseAddress(alphaBuffer, .readOnly)
//            CVPixelBufferUnlockBaseAddress(self, [])
            CVPixelBufferUnlockBaseAddress(copy, [])
            CVPixelBufferUnlockBaseAddress(self, .readOnly)
        }

        guard var dest = CVPixelBufferGetBaseAddress(copy) else { return  self }
        guard var source = CVPixelBufferGetBaseAddress(self) else { return self }
        guard var alphaPtr0 = CVPixelBufferGetBaseAddress(alphaBuffer) else { return self }
        let bytesPerRowSrc = CVPixelBufferGetBytesPerRow(self)
        let bytesPerRowDest = CVPixelBufferGetBytesPerRow(copy)
        let bytesPerRowAlpha = CVPixelBufferGetBytesPerRow(alphaBuffer)
        let len = min(bytesPerRowSrc, bytesPerRowDest)
        //let len = bytesPerRowSrc
        for _ in 0..<height {
            memcpy(dest, source, len)
            var alphaPtr = alphaPtr0
            var destPtr = dest + offset
            var alpha:UInt8
//            var b8:UInt8, g8:UInt8, r8:UInt8, alphaFrg:UInt8
            for _ in stride(from: offset, to: bytesPerRowDest, by: 4) {
                alpha = alphaPtr.load(as: UInt8.self)
                if alpha == 0 {
                    memcpy(destPtr,alphaPtr,1)
                }
                else if alpha != 255 {
                    // alpha * foreground + (1- alpha) * background
//                    alphaFrg = 255 - destPtr.load(as:UInt8.self)
//                    b8 = (destPtr-3).load(as: UInt8.self)
//                    g8 = (destPtr-2).load(as: UInt8.self)
//                    r8 = (destPtr-1).load(as: UInt8.self)
//                    alphaFrag* (bgColor.blueComponent, bgColor.
                    memcpy(destPtr,alphaPtr,1)
                }
                else {
                    memcpy(destPtr,alphaPtr,1)
                }
                alphaPtr+=1; destPtr+=4
            }
            source = source.advanced(by: bytesPerRowSrc)
            dest = dest.advanced(by: bytesPerRowDest)
            alphaPtr0 = alphaPtr0.advanced(by: bytesPerRowAlpha)
        }
        return copy
    }
    
    func copy() throws -> CVPixelBuffer {
        precondition(CFGetTypeID(self) == CVPixelBufferGetTypeID(), "copy() cannot be called on a non-CVPixelBuffer")

        var _copy: CVPixelBuffer?

        let width = CVPixelBufferGetWidth(self)
        let height = CVPixelBufferGetHeight(self)
        let formatType = CVPixelBufferGetPixelFormatType(self)
        let attachments = CVBufferCopyAttachments(self, .shouldPropagate)

        CVPixelBufferCreate(nil, width, height, formatType, attachments, &_copy)

        guard let copy = _copy else {
            throw "CVPixelBuffer.allocationFailed"
            //throw PixelBufferCopyError.allocationFailed
        }

        CVPixelBufferLockBaseAddress(self, .readOnly)
        CVPixelBufferLockBaseAddress(copy, [])

        defer {
            CVPixelBufferUnlockBaseAddress(copy, [])
            CVPixelBufferUnlockBaseAddress(self, .readOnly)
        }

        let pixelBufferPlaneCount: Int = CVPixelBufferGetPlaneCount(self)


        if pixelBufferPlaneCount == 0 {
            let dest = CVPixelBufferGetBaseAddress(copy)
            let source = CVPixelBufferGetBaseAddress(self)
            let height = CVPixelBufferGetHeight(self)
            let bytesPerRowSrc = CVPixelBufferGetBytesPerRow(self)
            let bytesPerRowDest = CVPixelBufferGetBytesPerRow(copy)
            if bytesPerRowSrc == bytesPerRowDest {
                memcpy(dest, source, height * bytesPerRowSrc)
            }else {
                var startOfRowSrc = source
                var startOfRowDest = dest
                for _ in 0..<height {
                    memcpy(startOfRowDest, startOfRowSrc, min(bytesPerRowSrc, bytesPerRowDest))
                    startOfRowSrc = startOfRowSrc?.advanced(by: bytesPerRowSrc)
                    startOfRowDest = startOfRowDest?.advanced(by: bytesPerRowDest)
                }
            }

        }else {
            for plane in 0 ..< pixelBufferPlaneCount {
                let dest        = CVPixelBufferGetBaseAddressOfPlane(copy, plane)
                let source      = CVPixelBufferGetBaseAddressOfPlane(self, plane)
                let height      = CVPixelBufferGetHeightOfPlane(self, plane)
                let bytesPerRowSrc = CVPixelBufferGetBytesPerRowOfPlane(self, plane)
                let bytesPerRowDest = CVPixelBufferGetBytesPerRowOfPlane(copy, plane)

                if bytesPerRowSrc == bytesPerRowDest {
                    memcpy(dest, source, height * bytesPerRowSrc)
                }else {
                    var startOfRowSrc = source
                    var startOfRowDest = dest
                    for _ in 0..<height {
                        memcpy(startOfRowDest, startOfRowSrc, min(bytesPerRowSrc, bytesPerRowDest))
                        startOfRowSrc = startOfRowSrc?.advanced(by: bytesPerRowSrc)
                        startOfRowDest = startOfRowDest?.advanced(by: bytesPerRowDest)
                    }
                }
            }
        }
        return copy
    }
}
