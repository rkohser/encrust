#ifndef CIMAGEBUILDER_H
#define CIMAGEBUILDER_H

#include <cv.h>
#include <SDL.h>
#include <map>

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

class CSettingsReader;

/** Class containing the built frame and additionnal informations.
*/
class CFrameInfo
{

public:

    CFrameInfo()
    {
        m_pcvFrame = 0;
        m_pcvAlphaNotMask3 = 0;
        m_bReleaseOnDelete = false;
    }

    CFrameInfo(bool bReleaseOnDelete)
    {
        m_pcvFrame = 0;
        m_pcvAlphaNotMask3 = 0;
        m_bReleaseOnDelete = bReleaseOnDelete;
    }

    CFrameInfo(IplImage* pcvFrame, IplImage* pcvAlphaNotMask3 = 0, bool bReleaseOnDelete = true)
    {
        m_pcvFrame          = pcvFrame;
        m_pcvAlphaNotMask3  = pcvAlphaNotMask3;
        m_bReleaseOnDelete  = bReleaseOnDelete;
    }

    ~CFrameInfo()
    {
        if(m_bReleaseOnDelete)
        {
            if(m_pcvFrame)          cvReleaseImage(&m_pcvFrame);
            if(m_pcvAlphaNotMask3)  cvReleaseImage(&m_pcvAlphaNotMask3);
        }
    }

    IplImage*   m_pcvFrame;
    IplImage*   m_pcvAlphaNotMask3;
    bool        m_bReleaseOnDelete;

};

class CImageBuilder
{

    struct SDLTeam{
        SDLTeam() {
            pSdlLogo = 0;
            pSdlTeam = 0;
            pSdlScore = 0;
        }
        ~SDLTeam() {
            if(pSdlLogo)    SDL_FreeSurface(pSdlLogo);
            if(pSdlTeam)    SDL_FreeSurface(pSdlTeam);
            if(pSdlScore)   SDL_FreeSurface(pSdlScore);
        }

        SDL_Surface* pSdlLogo;
        SDL_Surface* pSdlTeam;
        SDL_Surface* pSdlScore;
    };

public:

    typedef std::map<std::string, SDLTeam*>     SDLTeamMap;
    typedef std::map<std::string, CFrameInfo*>  FrameMap;

    CImageBuilder(const std::string& sSettingsFile);
    ~CImageBuilder();

    /** Initialises the Image builder.
    @return True on success.
    */
    bool init();

    /** Starts the capture, runs run();
    */
    bool start();

    /** Requests to stop the thread.
    */
    void stop();

    /** Extracts the built frames in a map.
    */
    FrameMap popFrames();

    /** Says if the encrusted frames have been updated.
    */
    bool toUpdate();

    /** Returns the settings reader.
    */
    inline const CSettingsReader* getSettingsReader() const { return m_pSettingsReader;}

private:
    /** Thread Loop.
    */
    void run();

    /** Pushes the built frames into the frame map.
    */
    void pushFrame(const std::string& sKey, CFrameInfo* pImage);

    /** Clears the frame map.
    */
    void clearFrames();

    /** Generates the frames from the settings read with the CSettingsReader.
    @return True on success.
    */
    bool generateFrames();

    /** Generates a simple rounded rectangle with text in it.
    */
    CFrameInfo* generateMessageFrame(const std::string& sMessage);

    /** Generates a team image.
    */
    CFrameInfo* generateTeamFrame(const SDLTeam* pTeam);

    /** Loads the team frame and process sizes.
    */
    SDLTeamMap* loadTeamFrames();

    /** Generates a team image.
    */
    CFrameInfo* generatePictureFrame(const std::string& sAdPath, bool bAlphaBlending = false, double dScale = 1.0);

    /** Updates the generation paraters.
    */
    void updateGeneration();

    /** Converts a SDL_Surface to a Iplimage whatever the number of channels.
    */
    IplImage* sdlToCv(SDL_Surface* surface, double dScale = 1.0);

    /** Converts a 3 or 4 channel SDL_Surface to a CFrameInfo, informing also the 3 layer not-channel for encrustation.
        Mostly used for PNG images.
    */
    CFrameInfo* pngToCv(SDL_Surface* surface, bool bAddBlending = false, double dScale = 1.0);

    /** Builds an 4-channel alpha frame
    */
    CFrameInfo* buildAlphaFrame(IplImage* pcvImage, IplImage* pAlphaFrame);

    /** Builds a 1 channel rounded box with anti aliased borders
    */
    IplImage* aaRoundedBox1(uint uWidth, uint uHeight, double dAlphaBlending = 0.8, bool bTeamFrame = false, uint uRadius = 5, uint uSmoothWindow = 3);

    std::string         m_sSettingsFile;
    CSettingsReader*    m_pSettingsReader;

    FrameMap            m_mFrames;
    boost::mutex*       m_pFrameMutex;

    bool                m_bIsAlive;
    boost::thread*      m_pThread;

    uint                m_uhMargin;
    uint                m_uvMargin;
    uint                m_uhTeamMargin;
    uint                m_uvTeamMargin;
    uint                m_uradius;
    uint                m_usmooth;
    uint                m_utextDpi;
    double              m_dlogoZoom;
    double              m_dteamZoom;
    double              m_dadZoom;

    int                 m_iGeneralTeamFrameHeight;
    int                 m_iGeneralLogoWidth;
    int                 m_iGeneralTeamNameWidth;
    int                 m_iGeneralScoreWidth;

    bool                m_bToUpdate;
};

#endif // CIMAGEBUILDER_H
