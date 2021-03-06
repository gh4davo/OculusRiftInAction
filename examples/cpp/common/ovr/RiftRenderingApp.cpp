#include "Common.h"


void RiftRenderingApp::initGl() {
    glewExperimental = true;
    glewInit();
    glGetError();
    //oglplus::DefaultFramebuffer().Bind(oglplus::Framebuffer::Target::Draw);

    ovrGLConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.OGL.Header.API = ovrRenderAPI_OpenGL;
    cfg.OGL.Header.BackBufferSize = ovr::fromGlm(hmdNativeResolution);
    cfg.OGL.Header.Multisample = 1;
    ON_WINDOWS([&]{
      cfg.OGL.Window = (HWND)getRenderWindow();
    });

    int distortionCaps =
      ovrDistortionCap_Chromatic
      | ovrDistortionCap_Vignette
      | ovrDistortionCap_Overdrive
      | ovrDistortionCap_TimeWarp;

    ON_LINUX([&]{
      distortionCaps |= ovrDistortionCap_LinuxDevFullscreen;
    });

    int configResult = ovrHmd_ConfigureRendering(hmd, &cfg.Config,
      distortionCaps, hmd->MaxEyeFov, eyeRenderDescs);
    assert(configResult);
    renderingConfigured = configResult;

    for_each_eye([&](ovrEyeType eye){
      const ovrEyeRenderDesc & erd = eyeRenderDescs[eye];
      ovrMatrix4f ovrPerspectiveProjection = ovrMatrix4f_Projection(erd.Fov, 0.01f, 100000.0f, true);
      projections[eye] = ovr::toGlm(ovrPerspectiveProjection);
      eyeOffsets[eye] = erd.HmdToEyeViewOffset;
    });

    // Allocate the frameBuffer that will hold the scene, and then be
    // re-rendered to the screen with distortion
    glm::uvec2 frameBufferSize = ovr::toGlm(eyeTextures[0].Header.TextureSize);
    for_each_eye([&](ovrEyeType eye) {
      eyeFramebuffers[eye] = FramebufferWrapperPtr(new FramebufferWrapper());
      eyeFramebuffers[eye]->init(frameBufferSize);
      ((ovrGLTexture&)(eyeTextures[eye])).OGL.TexId =
        oglplus::GetName(eyeFramebuffers[eye]->color);
    });
  }

RiftRenderingApp::RiftRenderingApp() {
    Platform::sleepMillis(200);
    if (!ovrHmd_ConfigureTracking(hmd,
      ovrTrackingCap_Orientation | ovrTrackingCap_Position | ovrTrackingCap_MagYawCorrection, 0)) {
      SAY_ERR("Could not attach to sensor device");
    }

    memset(eyeTextures, 0, 2 * sizeof(ovrGLTexture));

    for_each_eye([&](ovrEyeType eye){
      ovrSizei eyeTextureSize = ovrHmd_GetFovTextureSize(hmd, eye, hmd->MaxEyeFov[eye], 1.0f);
      ovrTextureHeader & eyeTextureHeader = eyeTextures[eye].Header;
      eyeTextureHeader.TextureSize = eyeTextureSize;
      eyeTextureHeader.RenderViewport.Size = eyeTextureSize;
      eyeTextureHeader.API = ovrRenderAPI_OpenGL;
    });
  }

RiftRenderingApp::~RiftRenderingApp() {
}

bool RiftRenderingApp::isRenderingConfigured() {
  return renderingConfigured;
}

static RateCounter rateCounter;

void RiftRenderingApp::draw() {
  ++frameCount;
  onFrameStart();
  rateCounter.startCounter();
  ovrHmd_BeginFrame(hmd, frameCount);
  MatrixStack & mv = Stacks::modelview();
  MatrixStack & pr = Stacks::projection();

  ovrPosef fetchPoses[2];
  ovrHmd_GetEyePoses(hmd, frameCount, eyeOffsets, fetchPoses, nullptr);
  static ovrEyeType lastEyeRendered = ovrEye_Count;
  for (int i = 0; i < 2; ++i) {
    ovrEyeType eye = currentEye = hmd->EyeRenderOrder[i];
    // Force us to alternate eyes if we aren't keeping up with the required framerate
    if (eye == lastEyeRendered) {
      continue;
    }
    // We want to ensure that we only update the pose we 
    // send to the SDK if we actually render this eye.
    eyePoses[eye] = fetchPoses[eye];

    lastEyeRendered = eye;
    Stacks::withPush(pr, mv, [&] {
      // Set up the per-eye projection matrix
      pr.top() = projections[eye];


      // Set up the per-eye modelview matrix
      // Apply the head pose
      glm::mat4 eyePose = ovr::toGlm(eyePoses[eye]);
      mv.preMultiply(glm::inverse(eyePose));

      // Render the scene to an offscreen buffer
      eyeFramebuffers[eye]->Bind();
      renderScene();
    });
    
    if (eyePerFrameMode) {
      break;
    }
  }
  // Restore the default framebuffer
  //oglplus::DefaultFramebuffer().Bind(oglplus::Framebuffer::Target::Draw);
  ovrHmd_EndFrame(hmd, eyePoses, eyeTextures);
  onFrameEnd();
  rateCounter.increment();
  if (rateCounter.elapsed() > 2.0f) {
    float fps = rateCounter.getRate();
    SAY("FPS: %0.2f", fps);
    rateCounter.reset();
  }
}


//int framecount = 0;
//long start = Platform::elapsedMillis();
//while (!glfwWindowShouldClose(window)) {
//  glfwPollEvents();
//  ++frame;
//  update();
//  draw();
//  finishFrame();
//  long now = Platform::elapsedMillis();
//  ++framecount;
//  if ((now - start) >= 2000) {
//  }
//}
//

