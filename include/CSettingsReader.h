#ifndef CSETTINGSREADER_H
#define CSETTINGSREADER_H

#include <iostream>
#include <map>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

class CSettingsReader
{

public:

    /** Structure containing the Positions informations.
    */
    struct Positions{
        Positions(){
            vRelTeamTop = 0.0;
            vRelInterTeam = 0.0;
            vRelLogoTop = 1.0;
            hRelLogoRight = 1.0;
            vRelBottom = 1.0;
            vRelAdMessage = 1.0;
        }

        double  vRelTeamTop;
        double  vRelInterTeam;
        double  vRelLogoTop;
        double  hRelLogoRight;
        double  vRelBottom;
        double  vRelAdMessage;
    };

    /** Structure containing the Generation parameters.
    */
    struct Generation{
        Generation(){
            hMargin  = 50;
            vMargin  = 10;
            hTeamMargin = 8;
            vTeamMargin = 8;
            radius = 12;
            smooth = 5;
            textDpi = 100;
            logoZoom = 1.0;
            teamZoom = 1.0;
            adZoom = 1.0;
        }

        uint hMargin;
        uint vMargin;
        uint hTeamMargin;
        uint vTeamMargin;
        uint radius;
        uint smooth;
        uint textDpi;
        double logoZoom;
        double teamZoom;
        double adZoom;
    };

    /** Structure containing the color informations.
    */
    struct Color{
        uint        uRed;
        uint        uGreen;
        uint        uBlue;
        double      dTransparency;
    };

    /** Structure containing the team informations.
    */
    struct Team{
        std::string     sName;
        std::string     sLogoPath;
        std::string     sScore;
    };

    typedef std::map<unsigned int, Team> TeamMap;

public:
    CSettingsReader(const std::string& sSettingsFile);
    ~CSettingsReader();

    /** Method parsing the settings file.
      @return True on success.
    */
    bool loadSettings();

    /** Method which dumps the settings in std::cerr.
    */
    void dumpSettings();

    /** Method checking if the file has been updated.
    @return True if changed.
    */
    inline bool hasChanged();

    /// Accessors
    inline const std::string&   getMessage() const      { return m_sMessage; }
    inline const std::string&   getLogoPath() const     { return m_sLogoPath; }
    inline const std::string&   getAdPath() const       { return m_sAdPath; }
    inline const TeamMap&       getTeamMap() const      { return m_mTeams; }
    inline const Color&         getColor() const        { return m_tColor; }
    inline const Positions&     getPositions() const    { return m_tPositions;}
    inline const Generation&    getGeneration() const   { return m_tGeneration;}

private:

    /** Method gets the sec part of the last time the file was saved.
    */
    inline unsigned long getLastSavedSec();

    /** Method getting the comprehensive path to the given file, according
        to the working dir.
    */
    std::string     getOKFilePath(const std::string& sFilePath);

    std::string     m_sSettingsFile;
    unsigned long   m_uLastSavedSec;

    /// Settings
    std::string     m_sMessage;
    std::string     m_sLogoPath;
    std::string     m_sAdPath;

    TeamMap         m_mTeams;
    Color           m_tColor;
    Positions       m_tPositions;
    Generation      m_tGeneration;
};

unsigned long CSettingsReader::getLastSavedSec()
{
    struct stat attrib;
    if(stat(m_sSettingsFile.c_str(),&attrib) == -1)
    {
        //If the file does not exist create it once
        FILE* f = fopen(m_sSettingsFile.c_str(),"w");
        fclose(f);
        stat(m_sSettingsFile.c_str(),&attrib);
    }
    return attrib.st_mtim.tv_sec;
}

bool CSettingsReader::hasChanged()
{
    unsigned long uFileDate = getLastSavedSec();
    bool bRet = (uFileDate != m_uLastSavedSec);
    if(bRet)
        m_uLastSavedSec = uFileDate;
    return bRet;
}

#endif // CSETTINGSREADER_H
