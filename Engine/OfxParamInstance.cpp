//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 *Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 *contact: immarespond at gmail dot com
 *
 */

#include "OfxParamInstance.h"
#include <iostream>

//ofx extension
#include <nuke/fnPublicOfxExtensions.h>

#include <ofxParametricParam.h>

#include "Engine/AppManager.h"

#include "Engine/Knob.h"
#include "Engine/KnobFactory.h"
#include "Engine/KnobFile.h"
#include "Engine/KnobTypes.h"
#include "Engine/OfxEffectInstance.h"
#include "Engine/OfxClipInstance.h"
#include "Engine/OfxImageEffectInstance.h"
#include "Engine/VideoEngine.h"
#include "Engine/ViewerInstance.h"
#include "Engine/Curve.h"
#include "Engine/OfxOverlayInteract.h"
#include "Engine/Format.h"
#include "Engine/Project.h"
#include "Engine/AppInstance.h"
using namespace Natron;


static std::string getParamLabel(OFX::Host::Param::Instance* param){
    std::string label = param->getProperties().getStringProperty(kOfxPropLabel);
    if(label.empty()){
        label = param->getProperties().getStringProperty(kOfxPropShortLabel);
    }
    if(label.empty()){
        label = param->getProperties().getStringProperty(kOfxPropLongLabel);
    }
    if(label.empty()){
        label = param->getName();
    }
    return label;
}

///anonymous namespace to handle keyframes communication support for Ofx plugins
/// in a generalized manner
namespace OfxKeyFrame{
    
    OfxStatus getNumKeys(boost::shared_ptr<Knob> knob,unsigned int &nKeys) {
        int sum = 0;
        for (int i = 0 ; i < knob->getDimension(); ++i) {
            sum += knob->getCurve(i)->keyFramesCount();
        }
        nKeys =  sum;
        return kOfxStatOK;
    }

    OfxStatus getKeyTime(boost::shared_ptr<Knob> knob,int nth, OfxTime& time)
    {
        if (nth < 0) {
            return kOfxStatErrBadIndex;
        }
        int dimension = 0;
        int indexSoFar = 0;
        while (dimension < knob->getDimension()) {
            const KeyFrameSet& set = knob->getCurve(dimension)->getKeyFrames();
            ++dimension;
            if (nth >= (int)(set.size() + indexSoFar)) {
                indexSoFar += set.size();
                continue;
            } else {
                KeyFrameSet::const_iterator it = set.begin();
                while (it != set.end()) {
                    if (indexSoFar == nth) {
                        time = it->getTime();
                        return kOfxStatOK;
                    }
                    ++indexSoFar;
                    ++it;
                }
            }
        }
        
        return kOfxStatErrBadIndex;
    }
    OfxStatus getKeyIndex(boost::shared_ptr<Knob> knob,OfxTime time, int direction, int& index) {
        int c = 0;
        for(int i = 0; i < knob->getDimension();++i){
            const KeyFrameSet& set = knob->getCurve(i)->getKeyFrames();
            for (KeyFrameSet::const_iterator it = set.begin(); it!=set.end(); ++it) {
                if(it->getTime() == time){
                    
                    if(direction == 0){
                        index = c;
                    }else if(direction < 0){
                        if(it == set.begin()){
                            index = -1;
                        }else{
                            index = c - 1;
                        }
                    }else{
                        KeyFrameSet::const_iterator next = it;
                        ++next;
                        if(next != set.end()){
                            index = c + 1;
                        }else{
                            index = -1;
                        }
                    }
                    
                    return kOfxStatOK;
                }
                ++c;
            }
            
        }
        return kOfxStatErrValue;
    }
    OfxStatus deleteKey(boost::shared_ptr<Knob> knob,OfxTime time) {
        for(int i = 0; i < knob->getDimension();++i){
            knob->deleteValueAtTime(time, i);
        }
        return kOfxStatOK;
    }
    OfxStatus deleteAllKeys(boost::shared_ptr<Knob> knob){
        for(int i = 0; i < knob->getDimension();++i){
            knob->removeAnimation(i);
        }
        return kOfxStatOK;
    }
}

////////////////////////// OfxPushButtonInstance /////////////////////////////////////////////////

OfxPushButtonInstance::OfxPushButtonInstance(OfxEffectInstance* node,
                                             OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::Param::PushbuttonInstance(descriptor, node->effectInstance())
{
    _knob = Natron::createKnob<Button_Knob>(node, getParamLabel(this));
    
}


// callback which should set enabled state as appropriate
void OfxPushButtonInstance::setEnabled(){
    _knob->setEnabled(getEnabled());
}

// callback which should set secret state as appropriate
void OfxPushButtonInstance::setSecret() {
    _knob->setSecret(getSecret());
}

boost::shared_ptr<Knob> OfxPushButtonInstance::getKnob() const {
    return _knob;
}

////////////////////////// OfxIntegerInstance /////////////////////////////////////////////////


OfxIntegerInstance::OfxIntegerInstance(OfxEffectInstance* node,OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::Param::IntegerInstance(descriptor, node->effectInstance())
{
    const OFX::Host::Property::Set &properties = getProperties();
    _knob = Natron::createKnob<Int_Knob>(node, getParamLabel(this));
    
    int min = properties.getIntProperty(kOfxParamPropMin);
    int max = properties.getIntProperty(kOfxParamPropMax);
    int def = properties.getIntProperty(kOfxParamPropDefault);
    int displayMin = properties.getIntProperty(kOfxParamPropDisplayMin);
    int displayMax = properties.getIntProperty(kOfxParamPropDisplayMax);
    _knob->setDisplayMinimum(displayMin);
    _knob->setDisplayMaximum(displayMax);
    
    _knob->setMinimum(min);
    _knob->setIncrement(1); // kOfxParamPropIncrement only exists for Double
    _knob->setMaximum(max);
    _knob->setDefaultValue<int>(def);
}
OfxStatus OfxIntegerInstance::get(int& v) {
    v = _knob->getValue<int>();
    return kOfxStatOK;
}
OfxStatus OfxIntegerInstance::get(OfxTime time, int& v) {
    v = _knob->getValueAtTime<int>(time);
    return kOfxStatOK;
}
OfxStatus OfxIntegerInstance::set(int v){
    _knob->setValue<int>(v);
    return kOfxStatOK;
}
OfxStatus OfxIntegerInstance::set(OfxTime time, int v){
    _knob->setValueAtTime<int>(time,v);
    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void OfxIntegerInstance::setEnabled(){
    _knob->setEnabled(getEnabled());
}

// callback which should set secret state as appropriate
void OfxIntegerInstance::setSecret() {
    _knob->setSecret(getSecret());
}

boost::shared_ptr<Knob> OfxIntegerInstance::getKnob() const{
    return _knob;
}


OfxStatus OfxIntegerInstance::getNumKeys(unsigned int &nKeys) const {
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}
OfxStatus OfxIntegerInstance::getKeyTime(int nth, OfxTime& time) const {
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}
OfxStatus OfxIntegerInstance::getKeyIndex(OfxTime time, int direction, int & index) const {
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}
OfxStatus OfxIntegerInstance::deleteKey(OfxTime time) {
    return OfxKeyFrame::deleteKey(_knob, time);
}
OfxStatus OfxIntegerInstance::deleteAllKeys(){
    return OfxKeyFrame::deleteAllKeys(_knob);
}

void OfxIntegerInstance::onKnobAnimationLevelChanged(int lvl)
{
    Natron::AnimationLevel l = (Natron::AnimationLevel)lvl;
    assert(l == Natron::NO_ANIMATION || getCanAnimate());
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::NO_ANIMATION);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::INTERPOLATED_VALUE);
}

void OfxIntegerInstance::setDisplayRange(){
    int displayMin = getProperties().getIntProperty(kOfxParamPropDisplayMin);
    int displayMax = getProperties().getIntProperty(kOfxParamPropDisplayMax);
    _knob->setDisplayMinimum(displayMin);
    _knob->setDisplayMaximum(displayMax);

}


////////////////////////// OfxDoubleInstance /////////////////////////////////////////////////

static void valueAccordingToType(bool toType,const std::string& doubleType,OfxEffectInstance* effect,
                                 double* inOut1stDim,double* inOutS2ndDim = NULL){
    if(doubleType == kOfxParamDoubleTypePlain ||
       doubleType == kOfxParamDoubleTypeAngle ||
       doubleType == kOfxParamDoubleTypeScale ||
       doubleType == kOfxParamDoubleTypeTime ||
       doubleType == kOfxParamDoubleTypeAbsoluteTime){
        //types not handled
        return;
    }else if(doubleType == kOfxParamDoubleTypeX ||
             doubleType == kOfxParamDoubleTypeXAbsolute ||
             doubleType == kOfxParamDoubleTypeNormalisedX ||
             doubleType == kOfxParamDoubleTypeNormalisedXAbsolute){ //< treat absolute as non-absolute...
        
        const Format& projectFormat = effect->getApp()->getProject()->getProjectDefaultFormat();
        if(toType){
            *inOut1stDim *= (double)projectFormat.width();
        }else{
            *inOut1stDim /= (double)projectFormat.width();
        }
        return;
    }else if(doubleType == kOfxParamDoubleTypeY ||
             doubleType == kOfxParamDoubleTypeYAbsolute ||
             doubleType == kOfxParamDoubleTypeNormalisedY ||
             doubleType == kOfxParamDoubleTypeNormalisedYAbsolute){ //< treat absolute as non-absolute...
        const Format& projectFormat = effect->getApp()->getProject()->getProjectDefaultFormat();
        if(toType){
            *inOut1stDim *= (double)projectFormat.height();
        }else{
            *inOut1stDim /= (double)projectFormat.height();
        }
        return;
    }else if(doubleType == kOfxParamDoubleTypeXY ||
             doubleType == kOfxParamDoubleTypeXYAbsolute ||
             doubleType == kOfxParamDoubleTypeNormalisedXY ||
             doubleType == kOfxParamDoubleTypeNormalisedXYAbsolute){
        assert(inOutS2ndDim);
        const Format& projectFormat = effect->getApp()->getProject()->getProjectDefaultFormat();
        if(toType){
            *inOut1stDim *= (double)projectFormat.width();
            *inOutS2ndDim *= (double)projectFormat.height();
        }else{
            *inOut1stDim /= (double)projectFormat.width();
            *inOutS2ndDim /= (double)projectFormat.height();
        }
    }
    
    
}

OfxDoubleInstance::OfxDoubleInstance(OfxEffectInstance* node,  OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::Param::DoubleInstance(descriptor,node->effectInstance())
, _node(node)
{
    
    const OFX::Host::Property::Set &properties = getProperties();

    const std::string& doubleType = properties.getStringProperty(kOfxParamPropDoubleType);

 
    if(doubleType == kOfxParamDoubleTypeX ||
       doubleType == kOfxParamDoubleTypeXAbsolute ||
       doubleType == kOfxParamDoubleTypeNormalisedX ||
       doubleType == kOfxParamDoubleTypeNormalisedXAbsolute ||
       doubleType == kOfxParamDoubleTypeY ||
       doubleType == kOfxParamDoubleTypeYAbsolute ||
       doubleType == kOfxParamDoubleTypeNormalisedY ||
       doubleType == kOfxParamDoubleTypeNormalisedYAbsolute ||
       doubleType == kOfxParamDoubleTypeXY ||
       doubleType == kOfxParamDoubleTypeXYAbsolute ||
       doubleType == kOfxParamDoubleTypeNormalisedXY ||
       doubleType == kOfxParamDoubleTypeNormalisedXYAbsolute){
        ///connect to this slot ONLY if the double is spatial double
        QObject::connect(node->getApp()->getProject().get(), SIGNAL(formatChanged(Format)), this, SLOT(onProjectFormatChanged(Format)));
        
    }
    
    
    _knob = Natron::createKnob<Double_Knob>(node, getParamLabel(this));
    
    double min = properties.getDoubleProperty(kOfxParamPropMin);
    double max = properties.getDoubleProperty(kOfxParamPropMax);
    double incr = properties.getDoubleProperty(kOfxParamPropIncrement);
    double def = properties.getDoubleProperty(kOfxParamPropDefault);
    int decimals = properties.getIntProperty(kOfxParamPropDigits);

    valueAccordingToType(true,doubleType, node, &min);
    valueAccordingToType(true,doubleType, node, &max);
    _knob->setMinimum(min);
    _knob->setMaximum(max);
    setDisplayRange();
    if(incr > 0) {
        valueAccordingToType(true,doubleType, node, &incr);
        _knob->setIncrement(incr);
    }
    if(decimals > 0) {
        _knob->setDecimals(decimals);
    }
    
    valueAccordingToType(true,getProperties().getStringProperty(kOfxParamPropDoubleType),_node,&def);
    _knob->setDefaultValue<double>(def);
    
}
OfxStatus OfxDoubleInstance::get(double& v){
    v = _knob->getValue<double>();
    valueAccordingToType(false,getProperties().getStringProperty(kOfxParamPropDoubleType),_node,&v);
    return kOfxStatOK;
}
OfxStatus OfxDoubleInstance::get(OfxTime time, double& v){
    v = _knob->getValueAtTime<double>(time);
    valueAccordingToType(false,getProperties().getStringProperty(kOfxParamPropDoubleType),_node,&v);
    return kOfxStatOK;
}
OfxStatus OfxDoubleInstance::set(double v) {
    valueAccordingToType(true,getProperties().getStringProperty(kOfxParamPropDoubleType),_node,&v);
    _knob->setValue<double>(v);
    return kOfxStatOK;
}
OfxStatus OfxDoubleInstance::set(OfxTime time, double v){
    valueAccordingToType(true,getProperties().getStringProperty(kOfxParamPropDoubleType),_node,&v);
    _knob->setValueAtTime<double>(time,v);
    return kOfxStatOK;
}

void OfxDoubleInstance::onProjectFormatChanged(const Format& /*f*/){
    double v;
    get(v); //get the current value
    set(v); //refresh using the valueAccordingToType function
    setDisplayRange();
}

OfxStatus OfxDoubleInstance::derive(OfxTime /*time*/, double& v) {
#pragma message WARN("TODO: Double, Double2D, Double3D, RGBA, RGB derive and integrate for animated params")
    if (isAnimated()) {
        return kOfxStatErrUnsupported;
    }
    v = 0;
    return kOfxStatOK;
}

OfxStatus OfxDoubleInstance::integrate(OfxTime time1, OfxTime time2, double& v) {
    if (isAnimated()) {
        return kOfxStatErrUnsupported;
    }
    v = _knob->getValue<double>() * (time2 - time1);
    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void OfxDoubleInstance::setEnabled(){
    _knob->setEnabled(getEnabled());
}

// callback which should set secret state as appropriate
void OfxDoubleInstance::setSecret() {
    _knob->setSecret(getSecret());
}

void OfxDoubleInstance::setDisplayRange(){
    const std::string& doubleType = getProperties().getStringProperty(kOfxParamPropDoubleType);
    double displayMin = getProperties().getDoubleProperty(kOfxParamPropDisplayMin);
    double displayMax = getProperties().getDoubleProperty(kOfxParamPropDisplayMax);
    valueAccordingToType(true,doubleType, _node, &displayMin);
    valueAccordingToType(true,doubleType, _node, &displayMax);
    _knob->setDisplayMinimum(displayMin);
    _knob->setDisplayMaximum(displayMax);
}

boost::shared_ptr<Knob> OfxDoubleInstance::getKnob() const{
    return _knob;
}

bool OfxDoubleInstance::isAnimated() const {
    return _knob->isAnimated(0);
}

OfxStatus OfxDoubleInstance::getNumKeys(unsigned int &nKeys) const {
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}
OfxStatus OfxDoubleInstance::getKeyTime(int nth, OfxTime& time) const {
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}
OfxStatus OfxDoubleInstance::getKeyIndex(OfxTime time, int direction, int & index) const {
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}
OfxStatus OfxDoubleInstance::deleteKey(OfxTime time) {
    return OfxKeyFrame::deleteKey(_knob, time);
}
OfxStatus OfxDoubleInstance::deleteAllKeys(){
    return OfxKeyFrame::deleteAllKeys(_knob);
}

void OfxDoubleInstance::onKnobAnimationLevelChanged(int lvl)
{
    Natron::AnimationLevel l = (Natron::AnimationLevel)lvl;
    assert(l == Natron::NO_ANIMATION || getCanAnimate());
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::NO_ANIMATION);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::INTERPOLATED_VALUE);
}


////////////////////////// OfxBooleanInstance /////////////////////////////////////////////////

OfxBooleanInstance::OfxBooleanInstance(OfxEffectInstance* node, OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::Param::BooleanInstance(descriptor,node->effectInstance())
{
    const OFX::Host::Property::Set &properties = getProperties();
    
    _knob = Natron::createKnob<Bool_Knob>(node, getParamLabel(this));
    int def = properties.getIntProperty(kOfxParamPropDefault);
    _knob->setDefaultValue<bool>((bool)def);
    
}
OfxStatus OfxBooleanInstance::get(bool& b){
    b = _knob->getValue<bool>();
    return kOfxStatOK;
}

OfxStatus OfxBooleanInstance::get(OfxTime /*time*/, bool& b) {
    assert(Bool_Knob::canAnimateStatic());
    b = _knob->getValue<bool>();
    return kOfxStatOK;
}

OfxStatus OfxBooleanInstance::set(bool b){
    _knob->setValue<bool>(b);
    return kOfxStatOK;
}

OfxStatus OfxBooleanInstance::set(OfxTime /*time*/, bool b){
    assert(Bool_Knob::canAnimateStatic());
    _knob->setValue<bool>(b);
    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void OfxBooleanInstance::setEnabled(){
    _knob->setEnabled(getEnabled());
}

// callback which should set secret state as appropriate
void OfxBooleanInstance::setSecret() {
    _knob->setSecret(getSecret());
}

boost::shared_ptr<Knob> OfxBooleanInstance::getKnob() const{
    return _knob;
}

OfxStatus OfxBooleanInstance::getNumKeys(unsigned int &nKeys) const {
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}
OfxStatus OfxBooleanInstance::getKeyTime(int nth, OfxTime& time) const {
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}
OfxStatus OfxBooleanInstance::getKeyIndex(OfxTime time, int direction, int & index) const {
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}
OfxStatus OfxBooleanInstance::deleteKey(OfxTime time) {
    return OfxKeyFrame::deleteKey(_knob, time);
}
OfxStatus OfxBooleanInstance::deleteAllKeys(){
    return OfxKeyFrame::deleteAllKeys(_knob);
}

void OfxBooleanInstance::onKnobAnimationLevelChanged(int lvl)
{
    Natron::AnimationLevel l = (Natron::AnimationLevel)lvl;
    assert(l == Natron::NO_ANIMATION || getCanAnimate());
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::NO_ANIMATION);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::INTERPOLATED_VALUE);
}


////////////////////////// OfxChoiceInstance /////////////////////////////////////////////////

OfxChoiceInstance::OfxChoiceInstance(OfxEffectInstance* node, OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::Param::ChoiceInstance(descriptor,node->effectInstance())
{
    const OFX::Host::Property::Set &properties = getProperties();
    
    
    _knob = Natron::createKnob<Choice_Knob>(node, getParamLabel(this));
    
    setOption(0); // this actually sets all the options
    
    int def = properties.getIntProperty(kOfxParamPropDefault);
    _knob->setDefaultValue<int>(def);
}
OfxStatus OfxChoiceInstance::get(int& v){
    v = _knob->getActiveEntry();
    return kOfxStatOK;
}
OfxStatus OfxChoiceInstance::get(OfxTime /*time*/, int& v) {
    assert(Choice_Knob::canAnimateStatic());
    v = _knob->getActiveEntry();
    return kOfxStatOK;
}

OfxStatus OfxChoiceInstance::set(int v){
    if (0 <= v && v < (int)_entries.size()) {
        _knob->setValue<int>(v);
        return kOfxStatOK;
    } else {
        return kOfxStatErrBadIndex;
    }
}

OfxStatus OfxChoiceInstance::set(OfxTime /*time*/, int v) {
    assert(!Choice_Knob::canAnimateStatic());
    if (0 <= v && v < (int)_entries.size()) {
        _knob->setValue<int>(v);
        return kOfxStatOK;
    } else {
        return kOfxStatErrBadIndex;
    }
}


// callback which should set enabled state as appropriate
void OfxChoiceInstance::setEnabled(){
    _knob->setEnabled(getEnabled());
}

// callback which should set secret state as appropriate
void OfxChoiceInstance::setSecret() {
    _knob->setSecret(getSecret());
}

void OfxChoiceInstance::setOption(int /*num*/) {
    int dim = getProperties().getDimension(kOfxParamPropChoiceOption);
    _entries.clear();
    std::vector<std::string> helpStrings;
    for (int i = 0; i < dim; ++i) {
        std::string str = getProperties().getStringProperty(kOfxParamPropChoiceOption,i);
        std::string help = getProperties().getStringProperty(kOfxParamPropChoiceLabelOption,i);

        _entries.push_back(str);
        helpStrings.push_back(help);
    }
    _knob->populate(_entries, helpStrings);
}

boost::shared_ptr<Knob> OfxChoiceInstance::getKnob() const{
    return _knob;
}


OfxStatus OfxChoiceInstance::getNumKeys(unsigned int &nKeys) const {
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}
OfxStatus OfxChoiceInstance::getKeyTime(int nth, OfxTime& time) const {
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}
OfxStatus OfxChoiceInstance::getKeyIndex(OfxTime time, int direction, int & index) const {
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}
OfxStatus OfxChoiceInstance::deleteKey(OfxTime time) {
    return OfxKeyFrame::deleteKey(_knob, time);
}
OfxStatus OfxChoiceInstance::deleteAllKeys(){
    return OfxKeyFrame::deleteAllKeys(_knob);
}

void OfxChoiceInstance::onKnobAnimationLevelChanged(int lvl)
{
    Natron::AnimationLevel l = (Natron::AnimationLevel)lvl;
    assert(l == Natron::NO_ANIMATION || getCanAnimate());
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::NO_ANIMATION);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::INTERPOLATED_VALUE);
}


////////////////////////// OfxRGBAInstance /////////////////////////////////////////////////

OfxRGBAInstance::OfxRGBAInstance(OfxEffectInstance* node,OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::Param::RGBAInstance(descriptor,node->effectInstance())
{
    const OFX::Host::Property::Set &properties = getProperties();
    _knob = Natron::createKnob<Color_Knob>(node, getParamLabel(this),4);
    double defR = properties.getDoubleProperty(kOfxParamPropDefault,0);
    double defG = properties.getDoubleProperty(kOfxParamPropDefault,1);
    double defB = properties.getDoubleProperty(kOfxParamPropDefault,2);
    double defA = properties.getDoubleProperty(kOfxParamPropDefault,3);
    _knob->setDefaultValue<double>(defR,0);
    _knob->setDefaultValue<double>(defG,1);
    _knob->setDefaultValue<double>(defB,2);
    _knob->setDefaultValue<double>(defA,3);
}
OfxStatus OfxRGBAInstance::get(double& r, double& g, double& b, double& a) {
    
    r = _knob->getValue<double>(0);
    g = _knob->getValue<double>(1);
    b = _knob->getValue<double>(2);
    a = _knob->getValue<double>(3);
    return kOfxStatOK;
}
OfxStatus OfxRGBAInstance::get(OfxTime time, double&r ,double& g, double& b, double& a) {
    r = _knob->getValueAtTime<double>(time,0);
    g = _knob->getValueAtTime<double>(time,1);
    b = _knob->getValueAtTime<double>(time,2);
    a = _knob->getValueAtTime<double>(time,3);
    return kOfxStatOK;
}
OfxStatus OfxRGBAInstance::set(double r,double g , double b ,double a){
    _knob->setValue<double>(r,0);
    _knob->setValue<double>(g,1);
    _knob->setValue<double>(b,2);
    _knob->setValue<double>(a,3);
    return kOfxStatOK;
}
OfxStatus OfxRGBAInstance::set(OfxTime time, double r ,double g,double b,double a){
    _knob->setValueAtTime<double>(time,r,0);
    _knob->setValueAtTime<double>(time,g,1);
    _knob->setValueAtTime<double>(time,b,2);
    _knob->setValueAtTime<double>(time,a,3);
    return kOfxStatOK;
}

OfxStatus OfxRGBAInstance::derive(OfxTime /*time*/, double&r ,double& g, double& b, double& a) {
    if (isAnimated()) {
        return kOfxStatErrUnsupported;
    }
    r = g = b = a = 0;
    return kOfxStatOK;
}

OfxStatus OfxRGBAInstance::integrate(OfxTime time1, OfxTime time2, double&r ,double& g, double& b, double& a) {
    if (isAnimated()) {
        return kOfxStatErrUnsupported;
    }
    r = _knob->getValue<double>(0) * (time2 - time1);
    g = _knob->getValue<double>(1) * (time2 - time1);
    b = _knob->getValue<double>(2) * (time2 - time1);
    a = _knob->getValue<double>(3) * (time2 - time1);
    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void OfxRGBAInstance::setEnabled(){
    _knob->setEnabled(getEnabled());
}

// callback which should set secret state as appropriate
void OfxRGBAInstance::setSecret() {
    _knob->setSecret(getSecret());
}

boost::shared_ptr<Knob> OfxRGBAInstance::getKnob() const{
    return _knob;
}

bool OfxRGBAInstance::isAnimated(int dimension) const {
    return _knob->isAnimated(dimension);
}

bool OfxRGBAInstance::isAnimated() const {
    return _knob->isAnimated(0) || _knob->isAnimated(1) || _knob->isAnimated(2) || _knob->isAnimated(3);
}

OfxStatus OfxRGBAInstance::getNumKeys(unsigned int &nKeys) const {
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}
OfxStatus OfxRGBAInstance::getKeyTime(int nth, OfxTime& time) const {
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}
OfxStatus OfxRGBAInstance::getKeyIndex(OfxTime time, int direction, int & index) const {
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}
OfxStatus OfxRGBAInstance::deleteKey(OfxTime time) {
    return OfxKeyFrame::deleteKey(_knob, time);
}
OfxStatus OfxRGBAInstance::deleteAllKeys(){
    return OfxKeyFrame::deleteAllKeys(_knob);
}

void OfxRGBAInstance::onKnobAnimationLevelChanged(int lvl)
{
    Natron::AnimationLevel l = (Natron::AnimationLevel)lvl;
    assert(l == Natron::NO_ANIMATION || getCanAnimate());
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::NO_ANIMATION);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::INTERPOLATED_VALUE);
}


////////////////////////// OfxRGBInstance /////////////////////////////////////////////////

OfxRGBInstance::OfxRGBInstance(OfxEffectInstance* node,  OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::Param::RGBInstance(descriptor,node->effectInstance())
{
    const OFX::Host::Property::Set &properties = getProperties();
    
    _knob = Natron::createKnob<Color_Knob>(node, getParamLabel(this),3);
    double defR = properties.getDoubleProperty(kOfxParamPropDefault,0);
    double defG = properties.getDoubleProperty(kOfxParamPropDefault,1);
    double defB = properties.getDoubleProperty(kOfxParamPropDefault,2);
    _knob->setDefaultValue<double>(defR, 0);
    _knob->setDefaultValue<double>(defG, 1);
    _knob->setDefaultValue<double>(defB, 2);
    
}
OfxStatus OfxRGBInstance::get(double& r, double& g, double& b) {
    r = _knob->getValue<double>(0);
    g = _knob->getValue<double>(1);
    b = _knob->getValue<double>(2);
    return kOfxStatOK;
}
OfxStatus OfxRGBInstance::get(OfxTime time, double& r, double& g, double& b) {
    r = _knob->getValueAtTime<double>(time,0);
    g = _knob->getValueAtTime<double>(time,1);
    b = _knob->getValueAtTime<double>(time,2);
    return kOfxStatOK;
}
OfxStatus OfxRGBInstance::set(double r,double g,double b){
    _knob->setValue<double>(r,0);
    _knob->setValue<double>(g,1);
    _knob->setValue<double>(b,2);
    return kOfxStatOK;
}
OfxStatus OfxRGBInstance::set(OfxTime time, double r,double g,double b){
    _knob->setValueAtTime<double>(time,r,0);
    _knob->setValueAtTime<double>(time,g,1);
    _knob->setValueAtTime<double>(time,b,2);
    return kOfxStatOK;
}

OfxStatus OfxRGBInstance::derive(OfxTime /*time*/, double&r ,double& g, double& b) {
    if (isAnimated()) {
        return kOfxStatErrUnsupported;
    }
    r = g = b  = 0;
    return kOfxStatOK;
}

OfxStatus OfxRGBInstance::integrate(OfxTime time1, OfxTime time2, double&r ,double& g, double& b) {
    if (isAnimated()) {
        return kOfxStatErrUnsupported;
    }
    r = _knob->getValue<double>(0) * (time2 - time1);
    g = _knob->getValue<double>(1) * (time2 - time1);
    b = _knob->getValue<double>(2) * (time2 - time1);
    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void OfxRGBInstance::setEnabled(){
    _knob->setEnabled(getEnabled());
}

// callback which should set secret state as appropriate
void OfxRGBInstance::setSecret() {
    _knob->setSecret(getSecret());
}

boost::shared_ptr<Knob> OfxRGBInstance::getKnob() const{
    return _knob;
}

bool OfxRGBInstance::isAnimated(int dimension) const {
    return _knob->isAnimated(dimension);
}

bool OfxRGBInstance::isAnimated() const {
    return _knob->isAnimated(0) || _knob->isAnimated(1) || _knob->isAnimated(2);
}

OfxStatus OfxRGBInstance::getNumKeys(unsigned int &nKeys) const {
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}
OfxStatus OfxRGBInstance::getKeyTime(int nth, OfxTime& time) const {
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}
OfxStatus OfxRGBInstance::getKeyIndex(OfxTime time, int direction, int & index) const {
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}
OfxStatus OfxRGBInstance::deleteKey(OfxTime time) {
    return OfxKeyFrame::deleteKey(_knob, time);
}
OfxStatus OfxRGBInstance::deleteAllKeys(){
    return OfxKeyFrame::deleteAllKeys(_knob);
}

void OfxRGBInstance::onKnobAnimationLevelChanged(int lvl)
{
    Natron::AnimationLevel l = (Natron::AnimationLevel)lvl;
    assert(l == Natron::NO_ANIMATION || getCanAnimate());
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::NO_ANIMATION);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::INTERPOLATED_VALUE);
}


////////////////////////// OfxDouble2DInstance /////////////////////////////////////////////////

OfxDouble2DInstance::OfxDouble2DInstance(OfxEffectInstance* node, OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::Param::Double2DInstance(descriptor,node->effectInstance())
, _node(node)
{
    const OFX::Host::Property::Set &properties = getProperties();
    const std::string& doubleType = properties.getStringProperty(kOfxParamPropDoubleType);

    if(doubleType == kOfxParamDoubleTypeX ||
       doubleType == kOfxParamDoubleTypeXAbsolute ||
       doubleType == kOfxParamDoubleTypeNormalisedX ||
       doubleType == kOfxParamDoubleTypeNormalisedXAbsolute ||
       doubleType == kOfxParamDoubleTypeY ||
       doubleType == kOfxParamDoubleTypeYAbsolute ||
       doubleType == kOfxParamDoubleTypeNormalisedY ||
       doubleType == kOfxParamDoubleTypeNormalisedYAbsolute ||
       doubleType == kOfxParamDoubleTypeXY ||
       doubleType == kOfxParamDoubleTypeXYAbsolute ||
       doubleType == kOfxParamDoubleTypeNormalisedXY ||
       doubleType == kOfxParamDoubleTypeNormalisedXYAbsolute){
        ///connect to this slot ONLY if the double is spatial double
        QObject::connect(node->getApp()->getProject().get(), SIGNAL(formatChanged(Format)), this, SLOT(onProjectFormatChanged(Format)));
        
    }
    const int dims = 2;
    
    
    _knob = Natron::createKnob<Double_Knob>(node, getParamLabel(this),dims);

    std::vector<double> minimum(dims);
    std::vector<double> maximum(dims);
    std::vector<double> increment(dims);
    std::vector<double> displayMins(dims);
    std::vector<double> displayMaxs(dims);
    std::vector<int> decimals(dims);
    boost::scoped_array<double> def(new double[dims]);
    
    // kOfxParamPropIncrement and kOfxParamPropDigits only have one dimension,
    // @see Descriptor::addNumericParamProps() in ofxhParam.cpp
    // @see gDoubleParamProps in ofxsPropertyValidation.cpp
    double incr = properties.getDoubleProperty(kOfxParamPropIncrement);
    int dig = properties.getIntProperty(kOfxParamPropDigits);
    for (int i=0; i < dims; ++i) {
        minimum[i] = properties.getDoubleProperty(kOfxParamPropMin,i);
        maximum[i] = properties.getDoubleProperty(kOfxParamPropMax,i);
        increment[i] = incr;
        decimals[i] = dig;
        def[i] = properties.getDoubleProperty(kOfxParamPropDefault,i);
    }
    
    valueAccordingToType(true, doubleType, node, &minimum[0],&minimum[1]);
    valueAccordingToType(true, doubleType, node, &maximum[0],&maximum[1]);
    valueAccordingToType(true, doubleType, node, &def[0],&def[1]);
    
    _knob->setMinimumsAndMaximums(minimum, maximum);
    setDisplayRange();
    _knob->setIncrement(increment);
    _knob->setDecimals(decimals);
    _knob->setDefaultValue<double>(def[0], 0);
    _knob->setDefaultValue<double>(def[1], 1);

}

OfxStatus OfxDouble2DInstance::get(double& x1, double& x2) {
    x1 = _knob->getValue<double>(0);
    x2 = _knob->getValue<double>(1);
    valueAccordingToType(false, getProperties().getStringProperty(kOfxParamPropDoubleType), _node, &x1,&x2);
    return kOfxStatOK;
}

OfxStatus OfxDouble2DInstance::get(OfxTime time, double& x1, double& x2) {
    x1 = _knob->getValueAtTime<double>(time,0);
    x2 = _knob->getValueAtTime<double>(time,1);
    valueAccordingToType(false, getProperties().getStringProperty(kOfxParamPropDoubleType), _node, &x1,&x2);
    return kOfxStatOK;
}

OfxStatus OfxDouble2DInstance::set(double x1,double x2){
    valueAccordingToType(true, getProperties().getStringProperty(kOfxParamPropDoubleType), _node, &x1,&x2);
    _knob->setValue<double>(x1,0);
    _knob->setValue<double>(x2,1);
	return kOfxStatOK;
}

OfxStatus OfxDouble2DInstance::set(OfxTime time,double x1,double x2){
    valueAccordingToType(true, getProperties().getStringProperty(kOfxParamPropDoubleType), _node, &x1,&x2);
    _knob->setValueAtTime<double>(time,x1,0);
    _knob->setValueAtTime<double>(time,x2,1);
	return kOfxStatOK;
}

OfxStatus OfxDouble2DInstance::derive(OfxTime /*time*/, double&x1 ,double& x2) {
    if (isAnimated()) {
        return kOfxStatErrUnsupported;
    }
    x1 = x2 = 0;
    return kOfxStatOK;
}

OfxStatus OfxDouble2DInstance::integrate(OfxTime time1, OfxTime time2, double&x1 ,double& x2) {
    if (isAnimated()) {
        return kOfxStatErrUnsupported;
    }
    x1 = _knob->getValue<double>(0) * (time2 - time1);
    x2 = _knob->getValue<double>(1) * (time2 - time1);
    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void OfxDouble2DInstance::setEnabled(){
    _knob->setEnabled(getEnabled());
}

// callback which should set secret state as appropriate
void OfxDouble2DInstance::setSecret() {
    _knob->setSecret(getSecret());
}

void OfxDouble2DInstance::setDisplayRange() {
    const std::string& doubleType = getProperties().getStringProperty(kOfxParamPropDoubleType);
    std::vector<double> displayMins(2);
    std::vector<double> displayMaxs(2);
    displayMins[0] = getProperties().getDoubleProperty(kOfxParamPropDisplayMin,0);
    displayMins[1] = getProperties().getDoubleProperty(kOfxParamPropDisplayMin,1);
    displayMaxs[0] = getProperties().getDoubleProperty(kOfxParamPropDisplayMax,0);
    displayMaxs[1] = getProperties().getDoubleProperty(kOfxParamPropDisplayMax,1);
    valueAccordingToType(true,doubleType, _node, &displayMins[0],&displayMins[1]);
    valueAccordingToType(true,doubleType, _node, &displayMaxs[0],&displayMaxs[1]);
    _knob->setDisplayMinimumsAndMaximums(displayMins, displayMaxs);
}

boost::shared_ptr<Knob> OfxDouble2DInstance::getKnob() const{
    return _knob;
}

bool OfxDouble2DInstance::isAnimated(int dimension) const {
    return _knob->isAnimated(dimension);
}

bool OfxDouble2DInstance::isAnimated() const {
    return _knob->isAnimated(0) || _knob->isAnimated(1);
}

OfxStatus OfxDouble2DInstance::getNumKeys(unsigned int &nKeys) const {
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}
OfxStatus OfxDouble2DInstance::getKeyTime(int nth, OfxTime& time) const {
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}
OfxStatus OfxDouble2DInstance::getKeyIndex(OfxTime time, int direction, int & index) const {
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}
OfxStatus OfxDouble2DInstance::deleteKey(OfxTime time) {
    return OfxKeyFrame::deleteKey(_knob, time);
}
OfxStatus OfxDouble2DInstance::deleteAllKeys(){
    return OfxKeyFrame::deleteAllKeys(_knob);
}

void OfxDouble2DInstance::onKnobAnimationLevelChanged(int lvl)
{
    Natron::AnimationLevel l = (Natron::AnimationLevel)lvl;
    assert(l == Natron::NO_ANIMATION || getCanAnimate());
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::NO_ANIMATION);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::INTERPOLATED_VALUE);
}

void OfxDouble2DInstance::onProjectFormatChanged(const Format& /*f*/){
    double v1,v2;
    get(v1,v2); //get the current value
    set(v1,v2); //refresh using the valueAccordingToType function
    setDisplayRange();
}


////////////////////////// OfxInteger2DInstance /////////////////////////////////////////////////

OfxInteger2DInstance::OfxInteger2DInstance(OfxEffectInstance *node, OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::Param::Integer2DInstance(descriptor,node->effectInstance())
{
    const int dims = 2;
    const OFX::Host::Property::Set &properties = getProperties();
    
    
    _knob = Natron::createKnob<Int_Knob>(node, getParamLabel(this), dims);
    
    std::vector<int> minimum(dims);
    std::vector<int> maximum(dims);
    std::vector<int> increment(dims);
    std::vector<int> displayMins(dims);
    std::vector<int> displayMaxs(dims);
    boost::scoped_array<int> def(new int[dims]);
    
    for (int i = 0; i < dims; ++i) {
        minimum[i] = properties.getIntProperty(kOfxParamPropMin,i);
        displayMins[i] = properties.getIntProperty(kOfxParamPropDisplayMin,i);
        displayMaxs[i] = properties.getIntProperty(kOfxParamPropDisplayMax,i);
        maximum[i] = properties.getIntProperty(kOfxParamPropMax,i);
        increment[i] = 1; // kOfxParamPropIncrement only exists for Double
        def[i] = properties.getIntProperty(kOfxParamPropDefault,i);
    }
    
    _knob->setMinimumsAndMaximums(minimum, maximum);
    _knob->setIncrement(increment);
    _knob->setDisplayMinimumsAndMaximums(displayMins, displayMaxs);
    _knob->setDefaultValue<int>(def[0], 0);
    _knob->setDefaultValue<int>(def[1], 1);

}

OfxStatus OfxInteger2DInstance::get(int& x1, int& x2) {
    x1 = _knob->getValue<int>(0);
    x2 = _knob->getValue<int>(1);
    return kOfxStatOK;
}

OfxStatus OfxInteger2DInstance::get(OfxTime time, int& x1, int& x2) {
    x1 = _knob->getValueAtTime<int>(time,0);
    x2 = _knob->getValueAtTime<int>(time,1);
    return kOfxStatOK;
}

OfxStatus OfxInteger2DInstance::set(int x1,int x2) {
    _knob->setValue<int>(x1,0);
    _knob->setValue<int>(x2,1);
	return kOfxStatOK;
}

OfxStatus OfxInteger2DInstance::set(OfxTime time, int x1, int x2) {
    _knob->setValueAtTime<int>(time,x1,0);
    _knob->setValueAtTime<int>(time,x2,1);
	return kOfxStatOK;
}


// callback which should set enabled state as appropriate
void OfxInteger2DInstance::setEnabled() {
    _knob->setEnabled(getEnabled());
}

// callback which should set secret state as appropriate
void OfxInteger2DInstance::setSecret() {
    _knob->setSecret(getSecret());
}

boost::shared_ptr<Knob> OfxInteger2DInstance::getKnob() const {
    return _knob;
}

OfxStatus OfxInteger2DInstance::getNumKeys(unsigned int &nKeys) const {
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}
OfxStatus OfxInteger2DInstance::getKeyTime(int nth, OfxTime& time) const {
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}
OfxStatus OfxInteger2DInstance::getKeyIndex(OfxTime time, int direction, int & index) const {
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}
OfxStatus OfxInteger2DInstance::deleteKey(OfxTime time) {
    return OfxKeyFrame::deleteKey(_knob, time);
}
OfxStatus OfxInteger2DInstance::deleteAllKeys(){
    return OfxKeyFrame::deleteAllKeys(_knob);
}

void OfxInteger2DInstance::onKnobAnimationLevelChanged(int lvl)
{
    Natron::AnimationLevel l = (Natron::AnimationLevel)lvl;
    assert(l == Natron::NO_ANIMATION || getCanAnimate());
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::NO_ANIMATION);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::INTERPOLATED_VALUE);
}


////////////////////////// OfxDouble3DInstance /////////////////////////////////////////////////

OfxDouble3DInstance::OfxDouble3DInstance(OfxEffectInstance* node, OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::Param::Double3DInstance(descriptor,node->effectInstance())
{
    const int dims = 3;
    const OFX::Host::Property::Set &properties = getProperties();
    
    
    _knob = Natron::createKnob<Double_Knob>(node, getParamLabel(this),dims);
    
    std::vector<double> minimum(dims);
    std::vector<double> maximum(dims);
    std::vector<double> increment(dims);
    std::vector<double> displayMins(dims);
    std::vector<double> displayMaxs(dims);
    std::vector<int> decimals(dims);
    boost::scoped_array<double> def(new double[dims]);
    
    // kOfxParamPropIncrement and kOfxParamPropDigits only have one dimension,
    // @see Descriptor::addNumericParamProps() in ofxhParam.cpp
    // @see gDoubleParamProps in ofxsPropertyValidation.cpp
    double incr = properties.getDoubleProperty(kOfxParamPropIncrement);
    int dig = properties.getIntProperty(kOfxParamPropDigits);
    for (int i = 0; i < dims; ++i) {
        minimum[i] = properties.getDoubleProperty(kOfxParamPropMin,i);
        displayMins[i] = properties.getDoubleProperty(kOfxParamPropDisplayMin,i);
        displayMaxs[i] = properties.getDoubleProperty(kOfxParamPropDisplayMax,i);
        maximum[i] = properties.getDoubleProperty(kOfxParamPropMax,i);
        increment[i] = incr;
        decimals[i] = dig;
        def[i] = properties.getDoubleProperty(kOfxParamPropDefault,i);
    }
    
    _knob->setMinimumsAndMaximums(minimum, maximum);
    _knob->setIncrement(increment);
    _knob->setDisplayMinimumsAndMaximums(displayMins, displayMaxs);
    _knob->setDecimals(decimals);
    _knob->setDefaultValue<double>(def[0],0);
    _knob->setDefaultValue<double>(def[1],1);
    _knob->setDefaultValue<double>(def[2],2);
}

OfxStatus OfxDouble3DInstance::get(double& x1, double& x2, double& x3) {
    x1 = _knob->getValue<double>(0);
    x2 = _knob->getValue<double>(1);
    x3 = _knob->getValue<double>(2);
    return kOfxStatOK;
}

OfxStatus OfxDouble3DInstance::get(OfxTime time, double& x1, double& x2, double& x3) {
    x1 = _knob->getValueAtTime<double>(time,0);
    x2 = _knob->getValueAtTime<double>(time,1);
    x3 = _knob->getValueAtTime<double>(time,2);
    return kOfxStatOK;
}

OfxStatus OfxDouble3DInstance::set(double x1,double x2,double x3){
    _knob->setValue<double>(x1,0);
    _knob->setValue<double>(x2,1);
    _knob->setValue<double>(x3,2);
	return kOfxStatOK;
}

OfxStatus OfxDouble3DInstance::set(OfxTime time, double x1, double x2, double x3) {
    _knob->setValueAtTime<double>(time,x1,0);
    _knob->setValueAtTime<double>(time,x2,1);
    _knob->setValueAtTime<double>(time,x3,2);
	return kOfxStatOK;
}

OfxStatus OfxDouble3DInstance::derive(OfxTime /*time*/, double&x1 ,double& x2,double& x3) {
    if (isAnimated()) {
        return kOfxStatErrUnsupported;
    }
    x1 = x2 = x3 = 0;
    return kOfxStatOK;
}

OfxStatus OfxDouble3DInstance::integrate(OfxTime time1, OfxTime time2, double&x1 ,double& x2,double& x3) {
    if (isAnimated()) {
        return kOfxStatErrUnsupported;
    }
    x1 = _knob->getValue<double>(0) * (time2 - time1);
    x2 = _knob->getValue<double>(1) * (time2 - time1);
    x3 = _knob->getValue<double>(2) * (time2 - time1);
    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void OfxDouble3DInstance::setEnabled() {
    _knob->setEnabled(getEnabled());
}

// callback which should set secret state as appropriate
void OfxDouble3DInstance::setSecret() {
    _knob->setSecret(getSecret());
}

boost::shared_ptr<Knob> OfxDouble3DInstance::getKnob() const {
    return _knob;
}

bool OfxDouble3DInstance::isAnimated(int dimension) const {
    return _knob->isAnimated(dimension);
}

bool OfxDouble3DInstance::isAnimated() const {
    return _knob->isAnimated(0) || _knob->isAnimated(1) || _knob->isAnimated(2);
}

OfxStatus OfxDouble3DInstance::getNumKeys(unsigned int &nKeys) const {
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}
OfxStatus OfxDouble3DInstance::getKeyTime(int nth, OfxTime& time) const {
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}
OfxStatus OfxDouble3DInstance::getKeyIndex(OfxTime time, int direction, int & index) const {
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}
OfxStatus OfxDouble3DInstance::deleteKey(OfxTime time) {
    return OfxKeyFrame::deleteKey(_knob, time);
}
OfxStatus OfxDouble3DInstance::deleteAllKeys(){
    return OfxKeyFrame::deleteAllKeys(_knob);
}

void OfxDouble3DInstance::onKnobAnimationLevelChanged(int lvl)
{
    Natron::AnimationLevel l = (Natron::AnimationLevel)lvl;
    assert(l == Natron::NO_ANIMATION || getCanAnimate());
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::NO_ANIMATION);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::INTERPOLATED_VALUE);
}


////////////////////////// OfxInteger3DInstance /////////////////////////////////////////////////

OfxInteger3DInstance::OfxInteger3DInstance(OfxEffectInstance *node, OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::Param::Integer3DInstance(descriptor,node->effectInstance())
{
    const int dims = 3;
    const OFX::Host::Property::Set &properties = getProperties();
    
    
    _knob = Natron::createKnob<Int_Knob>(node, getParamLabel(this), dims);
    
    std::vector<int> minimum(dims);
    std::vector<int> maximum(dims);
    std::vector<int> increment(dims);
    std::vector<int> displayMins(dims);
    std::vector<int> displayMaxs(dims);
    boost::scoped_array<int> def(new int[dims]);
    
    for (int i = 0; i < dims; ++i) {
        minimum[i] = properties.getIntProperty(kOfxParamPropMin,i);
        displayMins[i] = properties.getIntProperty(kOfxParamPropDisplayMin,i);
        displayMaxs[i] = properties.getIntProperty(kOfxParamPropDisplayMax,i);
        maximum[i] = properties.getIntProperty(kOfxParamPropMax,i);
        int incr = properties.getIntProperty(kOfxParamPropIncrement,i);
        increment[i] = incr != 0 ?  incr : 1;
        def[i] = properties.getIntProperty(kOfxParamPropDefault,i);
    }
    
    _knob->setMinimumsAndMaximums(minimum, maximum);
    _knob->setIncrement(increment);
    _knob->setDisplayMinimumsAndMaximums(displayMins, displayMaxs);
    _knob->setDefaultValue<int>(def[0],0);
    _knob->setDefaultValue<int>(def[1],1);
    _knob->setDefaultValue<int>(def[2],2);
}

OfxStatus OfxInteger3DInstance::get(int& x1, int& x2, int& x3) {
    x1 = _knob->getValue<int>(0);
    x2 = _knob->getValue<int>(1);
    x3 = _knob->getValue<int>(2);
    return kOfxStatOK;
}

OfxStatus OfxInteger3DInstance::get(OfxTime time, int& x1, int& x2, int& x3) {
    x1 = _knob->getValueAtTime<int>(time,0);
    x2 = _knob->getValueAtTime<int>(time,1);
    x3 = _knob->getValueAtTime<int>(time,2);
    return kOfxStatOK;
}

OfxStatus OfxInteger3DInstance::set(int x1, int x2, int x3) {
    _knob->setValue<int>(x1,0);
    _knob->setValue<int>(x2,1);
    _knob->setValue<int>(x3,2);
	return kOfxStatOK;
}

OfxStatus OfxInteger3DInstance::set(OfxTime time, int x1, int x2, int x3) {
    _knob->setValueAtTime<int>(time,x1,0);
    _knob->setValueAtTime<int>(time,x2,1);
    _knob->setValueAtTime<int>(time,x3,2);
	return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void OfxInteger3DInstance::setEnabled() {
    _knob->setEnabled(getEnabled());
}

// callback which should set secret state as appropriate
void OfxInteger3DInstance::setSecret() {
    _knob->setSecret(getSecret());
}

boost::shared_ptr<Knob> OfxInteger3DInstance::getKnob() const {
    return _knob;
}

OfxStatus OfxInteger3DInstance::getNumKeys(unsigned int &nKeys) const {
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}
OfxStatus OfxInteger3DInstance::getKeyTime(int nth, OfxTime& time) const {
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}
OfxStatus OfxInteger3DInstance::getKeyIndex(OfxTime time, int direction, int & index) const {
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}
OfxStatus OfxInteger3DInstance::deleteKey(OfxTime time) {
    return OfxKeyFrame::deleteKey(_knob, time);
}
OfxStatus OfxInteger3DInstance::deleteAllKeys(){
    return OfxKeyFrame::deleteAllKeys(_knob);
}

void OfxInteger3DInstance::onKnobAnimationLevelChanged(int lvl)
{
    Natron::AnimationLevel l = (Natron::AnimationLevel)lvl;
    assert(l == Natron::NO_ANIMATION || getCanAnimate());
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::NO_ANIMATION);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::INTERPOLATED_VALUE);
}


////////////////////////// OfxGroupInstance /////////////////////////////////////////////////

OfxGroupInstance::OfxGroupInstance(OfxEffectInstance* node,OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::Param::GroupInstance(descriptor,node->effectInstance())
, _groupKnob()
, _tabKnob()
{
    const OFX::Host::Property::Set &properties = getProperties();
    
    int isTab = properties.getIntProperty(kFnOfxParamPropGroupIsTab);
    if(isTab){
        _tabKnob = Natron::createKnob<Tab_Knob>(node, getParamLabel(this));
        
    }else{
        _groupKnob = Natron::createKnob<Group_Knob>(node, getParamLabel(this));
        int opened = properties.getIntProperty(kOfxParamPropGroupOpen);
        _groupKnob->setDefaultValue<bool>(opened);
    }
    
    
}
void OfxGroupInstance::addKnob(boost::shared_ptr<Knob> k) {
    if(_groupKnob){
        _groupKnob->addKnob(k);
    }else{
        _tabKnob->addKnob(k);
    }
}

boost::shared_ptr<Knob> OfxGroupInstance::getKnob() const{
    if(_groupKnob){
        return _groupKnob;
    }else{
        return _tabKnob;
    }
}


////////////////////////// OfxStringInstance /////////////////////////////////////////////////


OfxStringInstance::OfxStringInstance(OfxEffectInstance* node,OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::Param::StringInstance(descriptor,node->effectInstance())
, _node(node)
, _fileKnob()
, _outputFileKnob()
, _stringKnob()
, _pathKnob()
{
    const OFX::Host::Property::Set &properties = getProperties();
    std::string mode = properties.getStringProperty(kOfxParamPropStringMode);
    
    if (mode == kOfxParamStringIsFilePath) {
        
        int fileIsImage = (node->isReader() || node->isWriter()) && (getScriptName() == "filename");
        int fileIsOutput = !properties.getIntProperty(kOfxParamPropStringFilePathExists);
        int filePathSupportsImageSequences = getCanAnimate();
        
        
        if (!fileIsOutput) {
            _fileKnob = Natron::createKnob<File_Knob>(node, getParamLabel(this));
            if(fileIsImage){
                _fileKnob->setAsInputImage();
            }
            if (!filePathSupportsImageSequences) {
                _fileKnob->turnOffAnimation();
            }
        } else {
            _outputFileKnob = Natron::createKnob<OutputFile_Knob>(node, getParamLabel(this));
            if(fileIsImage){
                _outputFileKnob->setAsOutputImageFile();
            }
            if (!filePathSupportsImageSequences) {
                _outputFileKnob->turnOffAnimation();
            }
            
        }
    } else if (mode == kOfxParamStringIsDirectoryPath) {
        _pathKnob = Natron::createKnob<Path_Knob>(node, getParamLabel(this));
        _pathKnob->setMultiPath(false);
    } else if (mode == kOfxParamStringIsSingleLine || mode == kOfxParamStringIsLabel || mode == kOfxParamStringIsMultiLine) {
        
        _stringKnob = Natron::createKnob<String_Knob>(node, getParamLabel(this));
        if (mode == kOfxParamStringIsLabel) {
            _stringKnob->setEnabled(false);
        }
        if(mode == kOfxParamStringIsMultiLine) {
            _stringKnob->setAsMultiLine();
        }
    }
    std::string defaultVal = properties.getStringProperty(kOfxParamPropDefault).c_str();
    if (!defaultVal.empty()) {
        if (_fileKnob) {
            _fileKnob->setDefaultValue<QStringList>(QStringList(defaultVal.c_str()));
        } else if (_outputFileKnob) {
            _outputFileKnob->setDefaultValue<QString>(QString(defaultVal.c_str()));
        } else if (_stringKnob) {
            _stringKnob->setDefaultValue<QString>(QString(defaultVal.c_str()));
        } else if (_pathKnob) {
            _pathKnob->setDefaultValue<QString>(QString(defaultVal.c_str()));
        }
    }
}

OfxStatus OfxStringInstance::get(std::string &str) {
    assert(_node->effectInstance());
    int currentFrame = (int)_node->effectInstance()->timeLineGetTime();
    if(_fileKnob){
        QString fileName =  _fileKnob->getRandomFrameName(currentFrame,true);
        str = fileName.toStdString();
    }else if(_outputFileKnob){
        str = _outputFileKnob->getValue<QString>().toStdString();
    }else if(_stringKnob){
        str = _stringKnob->getValueAtTime(currentFrame,0).toString().toStdString();
    }else if(_pathKnob){
        str = _pathKnob->getValue<QString>().toStdString();
    }
    return kOfxStatOK;
}

OfxStatus OfxStringInstance::get(OfxTime time, std::string& str) {
    assert(_node->effectInstance());
    if(_fileKnob){
        str = _fileKnob->getRandomFrameName(time,true).toStdString();
    }else if(_outputFileKnob){
        str = _outputFileKnob->getValue<QString>().toStdString();
    }else if(_stringKnob){
        str = _stringKnob->getValueAtTime(std::floor(time + 0.5), 0).toString().toStdString();
    }else if(_pathKnob){
        str = _pathKnob->getValue<QString>().toStdString();
    }
    return kOfxStatOK;
}

OfxStatus OfxStringInstance::set(const char* str) {
    if(_fileKnob){
        _fileKnob->setValue(str);
    }
    if(_outputFileKnob){
        _outputFileKnob->setValue(str);
    }
    if(_stringKnob){
        _stringKnob->setValue(str);
    }
    if(_pathKnob){
        _pathKnob->setValue(str);
    }
    return kOfxStatOK;
}

OfxStatus OfxStringInstance::set(OfxTime time, const char* str) {
    assert(!String_Knob::canAnimateStatic());
    if(_fileKnob){
        _fileKnob->setValueAtTime<QString>(time,QString(str));
    }
    if(_outputFileKnob){
        _outputFileKnob->setValue(str);
    }
    if(_stringKnob){
        _stringKnob->setValueAtTime((int)time,Variant(QString(str)),0);
    }
    if (_pathKnob) {
        _pathKnob->setValue(str);
    }
    return kOfxStatOK;
}
OfxStatus OfxStringInstance::getV(va_list arg){
    const char **value = va_arg(arg, const char **);
    
    OfxStatus stat = get(_localString.localData());
    *value = _localString.localData().c_str();
    return stat;
    
}
OfxStatus OfxStringInstance::getV(OfxTime time, va_list arg){
    const char **value = va_arg(arg, const char **);
    
    OfxStatus stat = get(time,_localString.localData());
    *value = _localString.localData().c_str();
    return stat;
}

boost::shared_ptr<Knob> OfxStringInstance::getKnob() const{
    
    if(_fileKnob){
        return _fileKnob;
    }
    if(_outputFileKnob){
        return _outputFileKnob;
    }
    if(_stringKnob){
        return _stringKnob;
    }
    if (_pathKnob) {
        return _pathKnob;
    }

    return boost::shared_ptr<Knob>();
}
// callback which should set enabled state as appropriate
void OfxStringInstance::setEnabled(){
    if(_fileKnob){
        _fileKnob->setEnabled(getEnabled());
    }
    if (_outputFileKnob) {
        _outputFileKnob->setEnabled(getEnabled());
    }
    if (_stringKnob) {
        _stringKnob->setEnabled(getEnabled());
    }
    if (_pathKnob) {
        _pathKnob->setEnabled(getEnabled());
    }
}

// callback which should set secret state as appropriate
void OfxStringInstance::setSecret(){
    if(_fileKnob){
        _fileKnob->setSecret(getSecret());
    }
    if (_outputFileKnob) {
        _outputFileKnob->setSecret(getSecret());
    }
    if (_stringKnob) {
        _stringKnob->setSecret(getSecret());
    }
    if (_pathKnob) {
        _pathKnob->setSecret(getSecret());
    }
}


const QString OfxStringInstance::getRandomFrameName(int f) const{
    return _fileKnob ? _fileKnob->getRandomFrameName(f,true) : "";
}


OfxStatus OfxStringInstance::getNumKeys(unsigned int &nKeys) const {
    boost::shared_ptr<Knob> knob;
    if(_stringKnob){
        knob = boost::dynamic_pointer_cast<Knob>(_stringKnob);
    } else {
#pragma message WARN("getNumKeys() should not return kOfxStatErrUnsupported!!! why not return nKeys=0???")
        return kOfxStatErrUnsupported;
    }
    return OfxKeyFrame::getNumKeys(knob, nKeys);
}
OfxStatus OfxStringInstance::getKeyTime(int nth, OfxTime& time) const {
    boost::shared_ptr<Knob> knob;
    if(_stringKnob){
        knob = boost::dynamic_pointer_cast<Knob>(_stringKnob);
    } else {
#pragma message WARN("getKeyTime() should not return kOfxStatErrUnsupported!!!")
        return kOfxStatErrUnsupported;
    }
    return OfxKeyFrame::getKeyTime(knob, nth, time);
}
OfxStatus OfxStringInstance::getKeyIndex(OfxTime time, int direction, int & index) const {
    boost::shared_ptr<Knob> knob;
    if(_stringKnob){
        knob = boost::dynamic_pointer_cast<Knob>(_stringKnob);
    } else {
#pragma message WARN("getKeyIndex() should not return kOfxStatErrUnsupported!!!")
        return kOfxStatErrUnsupported;
    }
    return OfxKeyFrame::getKeyIndex(knob, time, direction, index);
}
OfxStatus OfxStringInstance::deleteKey(OfxTime time) {
    boost::shared_ptr<Knob> knob;
    if(_stringKnob){
        knob = boost::dynamic_pointer_cast<Knob>(_stringKnob);
    } else {
#pragma message WARN("deleteKey() should not return kOfxStatErrUnsupported!!!")
        return kOfxStatErrUnsupported;
    }
    return OfxKeyFrame::deleteKey(knob, time);
}
OfxStatus OfxStringInstance::deleteAllKeys(){
    boost::shared_ptr<Knob> knob;
    if(_stringKnob){
        knob = boost::dynamic_pointer_cast<Knob>(_stringKnob);
    } else {
#pragma message WARN("deleteAllKeys() should not return kOfxStatErrUnsupported!!!")
        return kOfxStatErrUnsupported;
    }
    return OfxKeyFrame::deleteAllKeys(knob);
}

void OfxStringInstance::onKnobAnimationLevelChanged(int lvl)
{
    Natron::AnimationLevel l = (Natron::AnimationLevel)lvl;
    assert(l == Natron::NO_ANIMATION || getCanAnimate());
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::NO_ANIMATION);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::INTERPOLATED_VALUE);
}


////////////////////////// OfxCustomInstance /////////////////////////////////////////////////


/*
 http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#kOfxParamTypeCustom
 
 Custom parameters contain null terminated char * C strings, and may animate. They are designed to provide plugins with a way of storing data that is too complicated or impossible to store in a set of ordinary parameters.
 
 If a custom parameter animates, it must set its kOfxParamPropCustomInterpCallbackV1 property, which points to a OfxCustomParamInterpFuncV1 function. This function is used to interpolate keyframes in custom params.
 
 Custom parameters have no interface by default. However,
 
 * if they animate, the host's animation sheet/editor should present a keyframe/curve representation to allow positioning of keys and control of interpolation. The 'normal' (ie: paged or hierarchical) interface should not show any gui.
 * if the custom param sets its kOfxParamPropInteractV1 property, this should be used by the host in any normal (ie: paged or hierarchical) interface for the parameter.
 
 Custom parameters are mandatory, as they are simply ASCII C strings. However, animation of custom parameters an support for an in editor interact is optional.
 */



OfxCustomInstance::OfxCustomInstance(OfxEffectInstance* node,OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::Param::CustomInstance(descriptor,node->effectInstance())
, _node(node)
, _knob()
, _customParamInterpolationV1Entry(0)
{
    const OFX::Host::Property::Set &properties = getProperties();
    
    
    _knob = Natron::createKnob<String_Knob>(node, getParamLabel(this));
    _knob->setAsCustom();
    
    _knob->setDefaultValue<QString>(properties.getStringProperty(kOfxParamPropDefault).c_str());
    
    _customParamInterpolationV1Entry = (customParamInterpolationV1Entry_t)properties.getPointerProperty(kOfxParamPropCustomInterpCallbackV1);
    if (_customParamInterpolationV1Entry) {
        _knob->setCustomInterpolation(_customParamInterpolationV1Entry, (void*)getHandle());
    }
}

OfxStatus OfxCustomInstance::get(std::string &str) {
    assert(_node->effectInstance());
    int currentFrame = (int)_node->effectInstance()->timeLineGetTime();
    str = _knob->getValueAtTime(currentFrame, 0).toString().toStdString();
    return kOfxStatOK;
}

OfxStatus OfxCustomInstance::get(OfxTime time, std::string& str) {
    assert(String_Knob::canAnimateStatic());
    // it should call _customParamInterpolationV1Entry
    assert(_node->effectInstance());
    str = _knob->getValueAtTime(time, 0).toString().toStdString();
    return kOfxStatOK;
}


OfxStatus OfxCustomInstance::set(const char* str) {
    _knob->setValue<QString>(QString(str));
    return kOfxStatOK;
}

OfxStatus OfxCustomInstance::set(OfxTime time, const char* str) {
    assert(String_Knob::canAnimateStatic());
    _knob->setValueAtTime(time,Variant(QString(str)),0);
    return kOfxStatOK;
}

OfxStatus OfxCustomInstance::getV(va_list arg) {
    const char **value = va_arg(arg, const char **);
    
    OfxStatus stat = get(_localString.localData());
    *value = _localString.localData().c_str();
    return stat;
    
}
OfxStatus OfxCustomInstance::getV(OfxTime time, va_list arg) {
    const char **value = va_arg(arg, const char **);
    
    OfxStatus stat = get(time,_localString.localData());
    *value = _localString.localData().c_str();
    return stat;
}

boost::shared_ptr<Knob> OfxCustomInstance::getKnob() const {
    return _knob;
}

// callback which should set enabled state as appropriate
void OfxCustomInstance::setEnabled() {
    _knob->setEnabled(getEnabled());
}

// callback which should set secret state as appropriate
void OfxCustomInstance::setSecret() {
    _knob->setSecret(getSecret());
}

OfxStatus OfxCustomInstance::getNumKeys(unsigned int &nKeys) const {
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}
OfxStatus OfxCustomInstance::getKeyTime(int nth, OfxTime& time) const {
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}
OfxStatus OfxCustomInstance::getKeyIndex(OfxTime time, int direction, int & index) const {
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}
OfxStatus OfxCustomInstance::deleteKey(OfxTime time) {
    return OfxKeyFrame::deleteKey(_knob, time);
}
OfxStatus OfxCustomInstance::deleteAllKeys(){
    return OfxKeyFrame::deleteAllKeys(_knob);
}

void OfxCustomInstance::onKnobAnimationLevelChanged(int lvl)
{
    Natron::AnimationLevel l = (Natron::AnimationLevel)lvl;
    assert(l == Natron::NO_ANIMATION || getCanAnimate());
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::NO_ANIMATION);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::INTERPOLATED_VALUE);
}


////////////////////////// OfxParametricInstance /////////////////////////////////////////////////

OfxParametricInstance::OfxParametricInstance(OfxEffectInstance* node, OFX::Host::Param::Descriptor& descriptor)
: OFX::Host::ParametricParam::ParametricInstance(descriptor,node->effectInstance())
, _descriptor(descriptor)
, _overlayInteract(NULL)
, _effect(node)
{
    
    
    
    const OFX::Host::Property::Set &properties = getProperties();
    int parametricDimension = properties.getIntProperty(kOfxParamPropParametricDimension);
    
    
    _knob = Natron::createKnob<Parametric_Knob>(node, getParamLabel(this),parametricDimension);
    
    setLabel();//set label on all curves
    
   std::vector<double> color(3*parametricDimension);
    properties.getDoublePropertyN(kOfxParamPropParametricUIColour, &color[0], 3*parametricDimension);
    
    for (int i = 0; i < parametricDimension; ++i) {
        _knob->setCurveColor(i, color[i*3], color[i*3+1], color[i*3+2]);
    }
    
    QObject::connect(_knob.get(),SIGNAL(mustInitializeOverlayInteract(CurveWidget*)),this,SLOT(initializeInteract(CurveWidget*)));
    QObject::connect(_knob.get(), SIGNAL(mustResetToDefault(QVector<int>)), this, SLOT(onResetToDefault(QVector<int>)));
    setDisplayRange();
}


void OfxParametricInstance::onResetToDefault(const QVector<int>& dimensions){
    for (int i = 0; i < dimensions.size(); ++i) {
        Natron::Status st = _knob->deleteAllControlPoints(dimensions.at(i));
        assert(st == Natron::StatOK);
        defaultInitializeFromDescriptor(dimensions.at(i),_descriptor);
    }
}

void OfxParametricInstance::initializeInteract(OverlaySupport* widget){
    
    OfxPluginEntryPoint* interactEntryPoint = (OfxPluginEntryPoint*)getProperties().getPointerProperty(kOfxParamPropParametricInteractBackground);
    if(interactEntryPoint){
        _overlayInteract = new Natron::OfxOverlayInteract((*_effect->effectInstance()),8,true);
        _overlayInteract->setCallingViewport(widget);
        _overlayInteract->createInstanceAction();
        QObject::connect(_knob.get(), SIGNAL(customBackgroundRequested()), this, SLOT(onCustomBackgroundDrawingRequested()));
    }
    
}

OfxParametricInstance::~OfxParametricInstance(){
    if(_overlayInteract){
        delete _overlayInteract;
    }
}


boost::shared_ptr<Knob> OfxParametricInstance::getKnob() const{
    return _knob;
}

// callback which should set enabled state as appropriate
void OfxParametricInstance::setEnabled() {
    _knob->setEnabled(getEnabled());
}

// callback which should set secret state as appropriate
void OfxParametricInstance::setSecret() {
    _knob->setSecret(getSecret());
}

/// callback which should update label
void OfxParametricInstance::setLabel() {
    _knob->setName(getParamLabel(this));
    for (int i = 0; i < _knob->getDimension(); ++i) {
        const std::string& curveName = getProperties().getStringProperty(kOfxParamPropDimensionLabel,i);
        _knob->setCurveLabel(i, curveName);
    }
}

void OfxParametricInstance::setDisplayRange() {
    double range_min = getProperties().getDoubleProperty(kOfxParamPropParametricRange,0);
    double range_max = getProperties().getDoubleProperty(kOfxParamPropParametricRange,1);
    
    assert(range_max > range_min);
    
    _knob->setParametricRange(range_min, range_max);
}

OfxStatus OfxParametricInstance::getValue(int curveIndex,OfxTime /*time*/,double parametricPosition,double *returnValue)
{
    Natron::Status stat = _knob->getValue(curveIndex, parametricPosition, returnValue);
    if(stat == Natron::StatOK){
        return kOfxStatOK;
    }else{
        return kOfxStatFailed;
    }
}

OfxStatus OfxParametricInstance::getNControlPoints(int curveIndex,double /*time*/,int *returnValue){
    Natron::Status stat = _knob->getNControlPoints(curveIndex, returnValue);
    if(stat == Natron::StatOK){
        return kOfxStatOK;
    }else{
        return kOfxStatFailed;
    }
}

OfxStatus OfxParametricInstance::getNthControlPoint(int curveIndex,
                                                    double /*time*/,
                                                    int    nthCtl,
                                                    double *key,
                                                    double *value) {
    Natron::Status stat = _knob->getNthControlPoint(curveIndex, nthCtl, key, value);
    if(stat == Natron::StatOK){
        return kOfxStatOK;
    }else{
        return kOfxStatFailed;
    }
}

OfxStatus OfxParametricInstance::setNthControlPoint(int   curveIndex,
                                                    double /*time*/,
                                                    int   nthCtl,
                                                    double key,
                                                    double value,
                                                    bool /*addAnimationKey*/
) {
    Natron::Status stat = _knob->setNthControlPoint(curveIndex, nthCtl, key, value);
    if(stat == Natron::StatOK){
        return kOfxStatOK;
    }else{
        return kOfxStatFailed;
    }
}
OfxStatus OfxParametricInstance::addControlPoint(int   curveIndex,
                                                 double /*time*/,
                                                 double key,
                                                 double value,
                                                 bool/* addAnimationKey*/) {
    Natron::Status stat = _knob->addControlPoint(curveIndex, key, value);
    if(stat == Natron::StatOK){
        return kOfxStatOK;
    }else{
        return kOfxStatFailed;
    }
}

OfxStatus  OfxParametricInstance::deleteControlPoint(int   curveIndex,int   nthCtl) {
    Natron::Status stat = _knob->deleteControlPoint(curveIndex, nthCtl);
    if(stat == Natron::StatOK){
        return kOfxStatOK;
    }else{
        return kOfxStatFailed;
    }
}

OfxStatus  OfxParametricInstance::deleteAllControlPoints(int   curveIndex) {
    Natron::Status stat = _knob->deleteAllControlPoints(curveIndex);
    if(stat == Natron::StatOK){
        return kOfxStatOK;
    }else{
        return kOfxStatFailed;
    }
}


void OfxParametricInstance::onCustomBackgroundDrawingRequested(){
    if(_overlayInteract){
        RenderScale s;
        _overlayInteract->getPixelScale(s.x, s.y);
        _overlayInteract->drawAction(_effect->effectInstance()->getFrameRecursive(), s);
    }
}
