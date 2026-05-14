/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
import SwiftUI
import RealityKit
import Metal

// Icons used by buttons below

// Flat button
/* SVG:
 <svg width="800" height="800" viewBox="0 0 800 800" fill="none" xmlns="http://www.w3.org/2000/svg">
 <path d="M133.333 400H666.667" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 </svg>
 */
struct FlatButtonIcon : Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        let width = rect.size.width
        let height = rect.size.height
        var strokePath = Path()
        strokePath.move(to: CGPoint(x: 0.16667*width, y: 0.5*height))
        strokePath.addLine(to: CGPoint(x: 0.83333*width, y: 0.5*height))
        path.addPath(strokePath.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        return path
    }
}

// Curved button
/* SVG:
 <svg width="800" height="800" viewBox="0 0 800 800" fill="none" xmlns="http://www.w3.org/2000/svg">
 <path d="M133 380C311 317.333 489 317.333 667 380" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 </svg>
 */
struct CurvedButtonIcon : Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        let width = rect.size.width
        let height = rect.size.height
        var strokePath = Path()
        strokePath.move(to: CGPoint(x: 0.16625*width, y: 0.475*height))
        strokePath.addCurve(to: CGPoint(x: 0.83375*width, y: 0.475*height), control1: CGPoint(x: 0.38875*width, y: 0.39667*height), control2: CGPoint(x: 0.61125*width, y: 0.39667*height))
        path.addPath(strokePath.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        return path
    }
}

// Curviest button
/* SVG:
 <svg width="800" height="800" viewBox="0 0 800 800" fill="none" xmlns="http://www.w3.org/2000/svg">
 <path d="M133 370C310.667 230 488.333 230 666 370" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 </svg>
 */
struct CurviestButtonIcon : Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        let width = rect.size.width
        let height = rect.size.height
        var strokePath = Path()
        strokePath.move(to: CGPoint(x: 0.16625*width, y: 0.4625*height))
        strokePath.addCurve(to: CGPoint(x: 0.8325*width, y: 0.4625*height), control1: CGPoint(x: 0.38833*width, y: 0.2875*height), control2: CGPoint(x: 0.61042*width, y: 0.2875*height))
        path.addPath(strokePath.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        return path
    }
}

/// UIHostingController subclass that hides the visionOS glass background.
internal class SDL_ClearHostingController<Content: View>: UIHostingController<Content> {
    override var preferredContainerBackgroundStyle: UIContainerBackgroundStyle {
        return .hidden
    }
}

/// ObjC-accessible wrapper that manages presenting SDL curved content
/// via a UIHostingController
@MainActor
@objc(SDL_CurvedContentHosting)
internal class SDL_CurvedContentHosting: NSObject {
    private let settings = SDL_CurvedContentSettings()

    private let helper = SDL_RealityKitHelper()

    private var hostingController: SDL_ClearHostingController<SDL_CurvedContentView>?

    @objc public override init() {
        //NSLog("SDL_CurvedContentHosting init")
        super.init()
    }

    /// Present the curved content view full-screen from the given view controller.
    /// Uses two-phase presentation: first bootstraps the RealityView as a hidden
    /// child VC, then presents modally (without animation) once content is ready.
    /// Modal presentation is required on visionOS to get an independent depth budget
    /// that doesn't clip curved mesh content extending forward from the window.
    @objc public func present(from viewController: UIViewController) {
        let contentView = SDL_CurvedContentView(helper: helper, settings: settings, onContentReady: { [weak self] in
            guard let self, let hc = self.hostingController else { return }

            hc.willMove(toParent: nil)
            hc.view.removeFromSuperview()
            hc.removeFromParent()
            hc.view.layer.opacity = 1

            //NSLog("SDL_CurvedContentHosting: RealityView content ready - presenting modally")
            viewController.present(hc, animated: false) { [weak self] in
                self?.updateOrnaments()
            }
        })

        // Spin up an async task to present / dismiss ornaments when there are updates to the scene state.
        let settings = self.settings
        let sceneStateObservations = Observations { [weak settings] in
            guard let settings else { return nil as (SDL_CurvedContentSettings.SceneState, SDL_CurvedContentSettings.InputType, Bool, Bool)? }
            return (settings.sceneState, settings.inputType, settings.isSnapped, settings.settingsExpanded)
        }
        Task { [weak self] in
            for await _ in sceneStateObservations {
                guard let self else { return }
                self.updateOrnaments()
            }
        }

        let hc = SDL_ClearHostingController(rootView: contentView)
        hc.modalPresentationStyle = .fullScreen
        hc.view.backgroundColor = .clear
        hostingController = hc

        hc.view.layer.opacity = 0
        viewController.addChild(hc)
        hc.view.frame = viewController.view.bounds
        viewController.view.addSubview(hc.view)
        hc.didMove(toParent: viewController)

        //NSLog("SDL_CurvedContentHosting: Bootstrapping RealityView as hidden child")
    }

    private func updateOrnaments() {
        guard let hostingController else { return }
        let settings = self.settings
        let sceneState = settings.sceneState
        UIView.animate(withDuration: 0.0) {
            if sceneState == .interactive {
                var sceneAnchor: UnitPoint
                var contentAlignment: Alignment
                if settings.isSnapped {
                    if settings.settingsExpanded {
                        sceneAnchor = .bottom
                        contentAlignment = .center
                    } else {
                        sceneAnchor = .bottom
                        contentAlignment = .top
                    }
                } else {
                    if settings.settingsExpanded {
                        sceneAnchor = .leading
                        contentAlignment = .center
                    } else {
                        sceneAnchor = .leading
                        contentAlignment = .trailing
                    }
                }
                hostingController.ornaments = [
                    UIHostingOrnament(sceneAnchor: sceneAnchor, contentAlignment: contentAlignment) {
                        SDL_SettingsPanelView(settings: settings)
                    }
                ]
            } else {
                hostingController.ornaments = []
            }
        }
    }

    /// Get the display texture for this frame.
    @objc public func getDisplayTexture(_ commandBuffer: MTLCommandBuffer, width: Int, height: Int, pixelFormat: MTLPixelFormat) -> MTLTexture? {
        return helper.getDisplayTexture(commandBuffer, width: width, height: height, pixelFormat: pixelFormat)
    }
}

// MARK: - Settings Panel

@Observable
internal class SDL_CurvedContentSettings {
    /// State of the app user interface, determined by the content view's state.
    enum SceneState {
        /// A state which allows the user to configure the scene.  Ornaments should be visible.
        case interactive

        /// A state which hides all UI except for the game itself.  Ornaments should not be visible.
        case cinematic
    }

    enum InputType {
        case eyes
        case pointer
    }

    var inputType: InputType = .eyes
    var showHover: Bool = true
    var isDimmed: Bool = false
    var curvatureRadius: Float = SDL_VisionOS_GetCurvature()
    var sceneState: SceneState = .interactive
    var isSnapped: Bool = false
    var settingsExpanded: Bool = false
}

struct SDL_SettingsPanelView: View {
    let settings: SDL_CurvedContentSettings
    @State private var curvatureSlider: Float = 0.0

    static let minimumCurvatureRadius: Float = 800.0
    static let maximumCurvatureRadius: Float = 4500.0

    static let curvatureSteps: [Float] = [
        0,
        4000,
        3000,
        2300,
        1800,
        1500,
        1000,
        800
    ]

    static let curvatureStepsSliderValue: [Float] = curvatureSteps.map {
        if $0 <= 0.01 {
            return 0 // flat
        }
        return 1.0 - ($0 - minimumCurvatureRadius) / (maximumCurvatureRadius - minimumCurvatureRadius)
    }

    private var curvatureLabel: String {
        if settings.curvatureRadius > 0 {
            return "\(Int(settings.curvatureRadius))R"
        } else {
            return ""
        }
    }

    var body: some View {
        if settings.settingsExpanded {
            expandedPanel
        } else {
            collapsedBar
        }
    }

    // MARK: Collapsed

    private var collapsedBar: some View {
        Button(action: { withAnimation { settings.settingsExpanded = true } }) {
            if settings.isSnapped {
                HStack(spacing: 12) {
                    Image(systemName: settings.showHover ? "eye" : "eye.slash")

                    Image(systemName: settings.isDimmed ? "moon.fill" : "sun.max")
                        .foregroundStyle(settings.isDimmed ? .primary : .secondary)

                    Divider().frame(height: 8)

                    if settings.curvatureRadius == 0 {
                        FlatButtonIcon()
                            .frame(width: 24, height: 24)
                    } else if settings.curvatureRadius > 1000.0 {
                        CurvedButtonIcon()
                            .frame(width: 24, height: 24)
                    } else {
                        CurviestButtonIcon()
                            .frame(width: 24, height: 24)
                    }
                }
                .padding(.horizontal, 16)
                .padding(.vertical, 16)
            } else {
                VStack(spacing: 12) {
                    Image(systemName: settings.showHover ? "eye" : "eye.slash")

                    Image(systemName: settings.isDimmed ? "moon.fill" : "sun.max")
                        .foregroundStyle(settings.isDimmed ? .primary : .secondary)

                    Divider().frame(height: 8)

                    if settings.curvatureRadius == 0 {
                        FlatButtonIcon()
                            .frame(width: 24, height: 24)
                    } else if settings.curvatureRadius > 1000.0 {
                        CurvedButtonIcon()
                            .frame(width: 24, height: 24)
                    } else {
                        CurviestButtonIcon()
                            .frame(width: 24, height: 24)
                    }
                }
                .padding(.horizontal, 16)
                .padding(.vertical, 16)
            }
        }
        .buttonStyle(.plain)
        .glassBackgroundEffect()
    }

    // MARK: Expanded

    private var expandedPanel: some View {
        VStack(spacing: 16) {
            // Input type and dim controls
            @Bindable var settings = self.settings

            Text("").font(.title).padding(8)

            HStack() {
                Spacer()
                Image(systemName: "eye.slash")

                Toggle(isOn: $settings.showHover) {
                }
                .labelsHidden()
                .tint(.secondary)

                Image(systemName: "eye")
                Spacer()

                Spacer()
                Image(systemName: "sun.max")

                Toggle(isOn: $settings.isDimmed) {
                }
                .labelsHidden()
                .tint(.secondary)

                Image(systemName: "moon.fill")
                Spacer()
            }

            // Curvature slider
            VStack(spacing: 4) {
                Text("\(curvatureLabel)")
                    .font(.caption)

                HStack() {
                    FlatButtonIcon()
                        .frame(width: 24, height: 24)

                    Slider(value: $curvatureSlider, in: 0...1) {
                    } currentValueLabel: {
                        Text("\(curvatureLabel)")
                    } ticks: {
                        SliderTickContentForEach(Self.curvatureStepsSliderValue, id: \.self) { value in
                            SliderTick(value)
                        }
                    }
                    .onAppear {
                        let curvature = settings.curvatureRadius
                        if curvature > 0 {
                            curvatureSlider = 1.0 - (curvature - Self.minimumCurvatureRadius)
                            / (Self.maximumCurvatureRadius - Self.minimumCurvatureRadius)
                        } else {
                            curvatureSlider = 0.0
                        }
                    }
                    .onChange(of: curvatureSlider) {
                        let clamped = max(0.0, min(1.0, curvatureSlider))
                        if clamped == 0 {
                            settings.curvatureRadius = 0
                        } else {
                            let radius = roundf(curvatureSlider * Self.minimumCurvatureRadius
                                                + (1.0 - curvatureSlider) * Self.maximumCurvatureRadius)
                            settings.curvatureRadius = radius
                        }
                        SDL_VisionOS_SendCurvatureChanged(settings.curvatureRadius)
                    }

                    CurviestButtonIcon()
                        .frame(width: 24, height: 24)
                }
            }
        }
        .padding(20)
        .frame(width: 340)
        .overlay(alignment: .topLeading) {
            // X button
            Button(action: { withAnimation { settings.settingsExpanded = false } }) {
                Image(systemName: "xmark")
                    .font(.system(size: 15, weight: .bold, design: .rounded))
                    .padding(8)
                    .contentShape(Circle())
            }
            .buttonStyle(.bordered)
            .buttonBorderShape(.circle)
            .padding(20)
        }
        .glassBackgroundEffect()
    }
}
