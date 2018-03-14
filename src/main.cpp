/*
 * main.cpp
 *
 *  Created on: 23 Feb. 2018
 *      Author: alan
 */

#include <memory>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <vector>
#include "VideoDetector.h"
#include "FaceListener.h"
#include "ProcessStatusListener.h"

using namespace std;
using namespace affdex;

namespace xl
{
namespace affdex
{

using namespace std;
using namespace affdex;

class App: public FaceListener, public ProcessStatusListener, public ImageListener
{
public:
    App()
    {
        /**
         The maximum number of frames processed per second
         If not specified, DEFAULT_PROCESSING_FRAMERATE=30
         */
        auto processFrameRate = 10;
        /**
         The maximum number of faces to track
         If not specified, DEFAULT_MAX_NUM_FACES=1
         */
        auto maxNumFaces = 1;
        /**
         Face detector configuration
         If not specified, defaults to FaceDetectorMode.SMALL_FACES

         FaceDetectorMode.LARGE_FACES=Faces occupying large portions of the photo
         FaceDetectorMode.SMALL_FACES=Faces occupying small portions of the photo
         */
        auto faceConfig = FaceDetectorMode::SMALL_FACES;

        m_detector = unique_ptr<VideoDetector>(new VideoDetector(processFrameRate, maxNumFaces, faceConfig));

        m_detector->setClassifierPath("../affdex-sdk/data/");

        m_detector->setDetectAllExpressions(true);
        m_detector->setDetectAllEmotions(true);
        m_detector->setDetectAllEmojis(true);
        m_detector->setDetectAllAppearances(true);

        m_detector->setFaceListener(this);
        m_detector->setImageListener(this);
        m_detector->setProcessStatusListener(this);

        m_detector->start();
    }

public:
    virtual void onFaceFound(float timestamp, FaceId faceId) override
    {
        cerr << "onFaceFound: " << timestamp << ", " << faceId << endl;
    }

    virtual void onFaceLost(float timestamp, FaceId faceId) override
    {
        cerr << "onFaceLost: " << timestamp << ", " << faceId << endl;
    }

    virtual void onImageResults(std::map<FaceId, Face> faces, Frame image)
    {
        ++m_frameCnt;
        if (m_frameCnt % 30 == 0)
        {
            cerr << "onImageResults(): m_frameCnt: " << m_frameCnt << endl;
        }

        auto& face = faces.begin()->second;

        if (m_csv.is_open()) {
            writeCsv(face);
        }
    }

    virtual void onImageCapture(Frame image) override
    {
//        cerr << "onImageCapture" << endl;
    }

    virtual void onProcessingException(AffdexException ex) override
    {
        cerr << "onFaceonProcessingException: " << ex.getExceptionMessage();
        cerr << endl;
    }

    virtual void onProcessingFinished() override
    {
        cerr << "onProcessingFinished. Total processed frame cnt: " << m_frameCnt << endl;
//        m_detector->stop();
        m_detector->reset();

        unique_lock<mutex> lock(m_mutex);
        m_done = true;
        m_csv.close();
        m_cond.notify_all();
    }

    virtual void processSync(const string& videoFile)
    {
        unique_lock<mutex> lock(m_mutex);
        m_detector->process(videoFile);
        m_done = false;
        m_cond.wait(lock, [this]{ return m_done; });
    }

    virtual void setOutputFile(const string& filename)
    {
        cerr << "Opening: " << filename << endl;
        m_csv.open(filename, std::fstream::out);
    }

protected:
    virtual void writeCsvImpl(const Emotions& t)
    {
        #define WRITE(field) \
            m_csvHeader.push_back(#field); \
            m_csvLine.push_back(boost::lexical_cast<string>(t.field))

        WRITE(joy);
        WRITE(fear);
        WRITE(disgust);
        WRITE(sadness);
        WRITE(anger);
        WRITE(surprise);
        WRITE(contempt);
        WRITE(valence);
        WRITE(engagement);
    }

    virtual void writeCsv(const Face& t) {
        // Write csv header on first line.
        auto writeHeader = m_csvHeader.empty();

        writeCsvImpl(t.emotions);

        if (writeHeader) {
            string s = boost::algorithm::join(m_csvHeader, ", ");
//            cerr << "csv: " << s << endl;
            m_csv << s << endl;
        }

        string s = boost::algorithm::join(m_csvLine, ", ");
//        cerr << "csv: " << s << endl;
        m_csv << s << endl;
        m_csvLine.clear();
    }

private:
    unique_ptr<VideoDetector> m_detector;
    int m_frameCnt = 0;
    mutex m_mutex;
    condition_variable m_cond;
    bool m_done = false;
    fstream m_csv;
    vector<string> m_csvHeader;
    vector<string> m_csvLine;
};

}
}

using namespace xl::affdex;

int main(int argc, char ** argsv)
{
    App app;
    app.setOutputFile("./out.csv");

    app.processSync("/home/alan/workspace/gaze_data/videos/emotions/alan/alan_emotions.webm");
}

