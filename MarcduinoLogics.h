////////////////

MARCDUINO_ACTION(FLDNormalSequence, @1T1, ({
                     FLD.selectSequence(LogicEngineRenderer::NORMAL);
                 }))

////////////////

MARCDUINO_ACTION(FLDFlashSequence, @1T2, ({
                     FLD.selectSequence(LogicEngineRenderer::FLASHCOLOR);
                 }))

////////////////

MARCDUINO_ACTION(FLDAlarmSequence, @1T3, ({
                     FLD.selectSequence(LogicEngineRenderer::ALARM);
                 }))

////////////////

MARCDUINO_ACTION(FLDFailureSequence, @1T4, ({
                     FLD.selectSequence(LogicEngineRenderer::FAILURE);
                 }))

////////////////

MARCDUINO_ACTION(FLDScreamLogicsSequence, @1T5, ({
                     FLD.selectSequence(LogicEngineRenderer::REDALERT);
                 }))

////////////////

MARCDUINO_ACTION(FLDLeiaLogicsSequence, @1T6, ({
                     FLD.selectSequence(LogicEngineRenderer::LEIA);
                 }))

////////////////

MARCDUINO_ACTION(FLDMarchSequence, @1T11, ({
                     FLD.selectSequence(LogicEngineRenderer::MARCH);
                 }))

////////////////

MARCDUINO_ACTION(RLDNormalSequence, @2T1, ({
                     RLD.selectSequence(LogicEngineRenderer::NORMAL);
                 }))

////////////////

MARCDUINO_ACTION(RLDFlashSequence, @2T2, ({
                     RLD.selectSequence(LogicEngineRenderer::FLASHCOLOR);
                 }))

////////////////

MARCDUINO_ACTION(RLDAlarmSequence, @2T3, ({
                     RLD.selectSequence(LogicEngineRenderer::ALARM);
                 }))

////////////////

MARCDUINO_ACTION(RLDFailureSequence, @2T4, ({
                     RLD.selectSequence(LogicEngineRenderer::FAILURE);
                 }))

////////////////

MARCDUINO_ACTION(RLDScreamLogicsSequence, @2T5, ({
                     RLD.selectSequence(LogicEngineRenderer::REDALERT);
                 }))

////////////////

MARCDUINO_ACTION(RLDLeiaLogicsSequence, @2T6, ({
                     RLD.selectSequence(LogicEngineRenderer::LEIA);
                 }))

////////////////

MARCDUINO_ACTION(RLDMarchSequence, @2T11, ({
                     RLD.selectSequence(LogicEngineRenderer::MARCH);
                 }))

////////////////

MARCDUINO_ACTION(NormalSequence, @0T1, ({
                     FLD.selectSequence(LogicEngineRenderer::NORMAL);
                     RLD.selectSequence(LogicEngineRenderer::NORMAL);
                 }))

////////////////

MARCDUINO_ACTION(FlashSequence, @0T2, ({
                     FLD.selectSequence(LogicEngineRenderer::FLASHCOLOR);
                     RLD.selectSequence(LogicEngineRenderer::FLASHCOLOR);
                 }))

////////////////

MARCDUINO_ACTION(AlarmSequence, @0T3, ({
                     FLD.selectSequence(LogicEngineRenderer::ALARM);
                     RLD.selectSequence(LogicEngineRenderer::ALARM);
                 }))

////////////////
// CREDENDA FORK IMPROVEMENT: Enhanced Failure sequence
// Added holo projector failure animation (HPA007) synchronized with logic displays
// Auto-resets to normal after 11 seconds

MARCDUINO_ACTION(FailureSequence, @0T4, ({
                     FLD.selectSequence(LogicEngineRenderer::FAILURE);
                     RLD.selectSequence(LogicEngineRenderer::FAILURE);
                     // Add All holo failure sequence
                     DO_COMMAND_AND_WAIT(F(
                                             "HPA007|11\n"),
                                         11000)
                     DO_RESET({
                         resetSequence();
                         // Reset to normal after failure
                         FLD.selectSequence(LogicEngineRenderer::NORMAL);
                         RLD.selectSequence(LogicEngineRenderer::NORMAL);
                     })
                 }))

////////////////
// CREDENDA FORK IMPROVEMENT: Enhanced Scream/Red Alert sequence
// Added holo projector scream animation (HPA0040) synchronized with logic displays
// Auto-resets to normal after 7 seconds

MARCDUINO_ACTION(ScreamLogicsSequence, @0T5, ({
                     FLD.selectSequence(LogicEngineRenderer::REDALERT);
                     RLD.selectSequence(LogicEngineRenderer::REDALERT);
                     // Add All holo scream sequence
                     DO_COMMAND_AND_WAIT(F(
                                             "HPA0040|7\n"),
                                         7000)
                     DO_RESET({
                         resetSequence();
                         // Reset to normal after scream
                         FLD.selectSequence(LogicEngineRenderer::NORMAL);
                         RLD.selectSequence(LogicEngineRenderer::NORMAL);
                     })
                 }))

////////////////
// CREDENDA FORK IMPROVEMENT: Enhanced Leia sequence
// Added front holo projector Leia message (HPS1) synchronized with logic displays
// Auto-resets after 45 seconds

MARCDUINO_ACTION(LeiaLogicsSequence, @0T6, ({
                     FLD.selectSequence(LogicEngineRenderer::LEIA);
                     RLD.selectSequence(LogicEngineRenderer::LEIA);
                     // Add Front holo Leia sequence
                     DO_COMMAND_AND_WAIT(F(
                                             "HPS1|45\n"),
                                         45000)
                     DO_RESET({
                         resetSequence();
                     })
                 }))

////////////////

MARCDUINO_ACTION(MarchSequence, @0T11, ({
                     FLD.selectSequence(LogicEngineRenderer::MARCH);
                     RLD.selectSequence(LogicEngineRenderer::MARCH);
                 }))

////////////////

MARCDUINO_ACTION(FLDRainbowSequence, @1T12, ({
                     // Front logic display rainbow showcase
                     FLD.selectSequence(LogicEngineRenderer::RAINBOW);
                 }))

////////////////

MARCDUINO_ACTION(RLDRainbowSequence, @2T12, ({
                     // Rear logic display rainbow showcase
                     RLD.selectSequence(LogicEngineRenderer::RAINBOW);
                 }))

////////////////

MARCDUINO_ACTION(RainbowSequence, @0T12, ({
                     // Combined front/rear rainbow showcase
                     FLD.selectSequence(LogicEngineRenderer::RAINBOW);
                     RLD.selectSequence(LogicEngineRenderer::RAINBOW);
                 }))

////////////////

MARCDUINO_ACTION(FLDLightsOutSequence, @1T15, ({
                     // Front logic fade-to-dark mode
                     FLD.selectSequence(LogicEngineRenderer::LIGHTSOUT);
                 }))

////////////////

MARCDUINO_ACTION(RLDLightsOutSequence, @2T15, ({
                     // Rear logic fade-to-dark mode
                     RLD.selectSequence(LogicEngineRenderer::LIGHTSOUT);
                 }))

////////////////

MARCDUINO_ACTION(LightsOutSequence, @0T15, ({
                     // Combined front/rear fade-to-dark mode
                     FLD.selectSequence(LogicEngineRenderer::LIGHTSOUT);
                     RLD.selectSequence(LogicEngineRenderer::LIGHTSOUT);
                 }))

////////////////

MARCDUINO_ACTION(FLDFireSequence, @1T22, ({
                     // Front logic fire renderer showcase
                     FLD.selectSequence(LogicEngineRenderer::FIRE);
                 }))

////////////////

MARCDUINO_ACTION(RLDFireSequence, @2T22, ({
                     // Rear logic fire renderer showcase
                     RLD.selectSequence(LogicEngineRenderer::FIRE);
                 }))

////////////////

MARCDUINO_ACTION(FireSequence, @0T22, ({
                     // Combined front/rear fire renderer showcase
                     FLD.selectSequence(LogicEngineRenderer::FIRE);
                     RLD.selectSequence(LogicEngineRenderer::FIRE);
                 }))

////////////////

MARCDUINO_ACTION(FLDPulseSequence, @1T24, ({
                     // Front logic pulse renderer showcase
                     FLD.selectSequence(LogicEngineRenderer::PULSE);
                 }))

////////////////

MARCDUINO_ACTION(RLDPulseSequence, @2T24, ({
                     // Rear logic pulse renderer showcase
                     RLD.selectSequence(LogicEngineRenderer::PULSE);
                 }))

////////////////

MARCDUINO_ACTION(PulseSequence, @0T24, ({
                     // Combined front/rear pulse renderer showcase
                     FLD.selectSequence(LogicEngineRenderer::PULSE);
                     RLD.selectSequence(LogicEngineRenderer::PULSE);
                 }))

////////////////

static char sMTFLDText[128];
static char sMBFLDText[128];
static char sMFLDText[256];
static char sMRLDText[256];

MARCDUINO_ACTION(TFLDScrollTextLeft, @1M, ({
                     snprintf(sMTFLDText, sizeof(sMTFLDText), "%s", Marcduino::getCommand());
                     snprintf(sMFLDText, sizeof(sMFLDText), "%s\n%s", sMTFLDText, sMBFLDText);
                     FLD.selectScrollTextLeft(sMFLDText, FLD.randomColor());
                 }))

////////////////

MARCDUINO_ACTION(BFLDScrollTextLeft, @2M, ({
                     snprintf(sMBFLDText, sizeof(sMBFLDText), "%s", Marcduino::getCommand());
                     snprintf(sMFLDText, sizeof(sMFLDText), "%s\n%s", sMTFLDText, sMBFLDText);
                     FLD.selectScrollTextLeft(sMFLDText, FLD.randomColor());
                 }))

////////////////

MARCDUINO_ACTION(RLDScrollTextLeft, @3M, ({
                     snprintf(sMRLDText, sizeof(sMRLDText), "%s", Marcduino::getCommand());
                     RLD.selectScrollTextLeft(sMRLDText, RLD.randomColor());
                 }))

////////////////

MARCDUINO_ACTION(TFLDTextLatin, @1P60, ({
                     FLD.setEffectFontNum(0);
                 }))

////////////////

MARCDUINO_ACTION(BFLDTextLatin, @2P60, ({
                     FLD.setEffectFontNum(0);
                 }))

////////////////

MARCDUINO_ACTION(RLDTextLatin, @3P60, ({
                     RLD.setEffectFontNum(0);
                 }))

////////////////

MARCDUINO_ACTION(TFLDTextAurabesh, @1P61, ({
                     FLD.setEffectFontNum(1);
                 }))

////////////////

MARCDUINO_ACTION(BFLDTextAurabesh, @2P61, ({
                     FLD.setEffectFontNum(1);
                 }))

////////////////

MARCDUINO_ACTION(RLDTextAurabesh, @3P61, ({
                     RLD.setEffectFontNum(1);
                 }))

////////////////
