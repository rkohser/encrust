#include <iostream>
#include <sstream>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/operations.hpp>
#include "tinyxml.h"

#include "CSettingsReader.h"

using namespace std;

CSettingsReader::CSettingsReader(const string &sSettingsFile)
{
    m_sSettingsFile = sSettingsFile;
    m_uLastSavedSec = 0;
}

CSettingsReader::~CSettingsReader()
{

}

bool CSettingsReader::loadSettings()
{
    m_mTeams.clear();

    TiXmlDocument doc(m_sSettingsFile);
    if (!doc.LoadFile())
    {
        stringstream buffer;
        buffer << doc.ErrorDesc();
        buffer << " (";
        buffer << doc.ErrorRow();
        buffer << ":";
        buffer << doc.ErrorCol();
        buffer << ")";
        cerr << "Error when loading file " << m_sSettingsFile << " : " << buffer.str() << endl;
        return false;
    }

    TiXmlHandle hDoc(&doc);
    TiXmlElement* pSettingsElem = 0, *pElem = 0;

    //<settings>
    TiXmlHandle hSettings = hDoc.FirstChildElement("settings");
    pSettingsElem = hSettings.Element();
    if(!pSettingsElem)
    {
        cerr << "No 'settings' balise !" << endl;
        return false;
    }

    // <message>
    pElem = pSettingsElem->FirstChildElement("message");
    if(!pElem || !pElem->GetText())
    {
        cerr << "No 'message' balise !" << endl;
        m_sMessage.clear();
    }
    else
    {
        m_sMessage = pElem->GetText();
    }

    // <logo>
    pElem = pSettingsElem->FirstChildElement("logo");
    if(!pElem || !pElem->GetText())
    {
        cerr << "No 'logo' balise !" << endl;
        m_sLogoPath.clear();
    }
    else
    {
        m_sLogoPath = getOKFilePath(pElem->GetText());
    }

    // <ad>
    pElem = pSettingsElem->FirstChildElement("ad");
    if(!pElem || !pElem->GetText())
    {
        cerr << "No 'ad' balise !" << endl;
        m_sAdPath.clear();
    }
    else
    {
        m_sAdPath = getOKFilePath(pElem->GetText());
    }

    // <color>
    TiXmlElement* pColorElem = pSettingsElem->FirstChildElement("color");
    if(!pColorElem)
    {
        cerr << "No 'color' balise , default to red !" << endl;
        memset(&m_tColor,0,sizeof(Color));
        m_tColor.uRed = 255;
        m_tColor.dTransparency = 0.8;
    }
    else
    {
        //transparency
        if(pColorElem->QueryDoubleAttribute("transparency", &(m_tColor.dTransparency)) != TIXML_SUCCESS)
        {
            cerr << "No 'transparency' attribute in color element!" << endl;
        }

        //red
        pElem = pColorElem->FirstChildElement("red");
        if(!pElem || !pElem->GetText())
        {
            cerr << "No 'red' balise in color element !" << endl;
        }
        else
        {
            m_tColor.uRed = strtoul(pElem->GetText(),0,10);
        }

        //green
        pElem = pColorElem->FirstChildElement("green");
        if(!pElem || !pElem->GetText())
        {
            cerr << "No 'green' balise in color element !" << endl;
        }
        else
        {
            m_tColor.uGreen = strtoul(pElem->GetText(),0,10);
        }

        //blue
        pElem = pColorElem->FirstChildElement("blue");
        if(!pElem || !pElem->GetText())
        {
            cerr << "No 'blue' balise in color element !" << endl;
        }
        else
        {
            m_tColor.uBlue = strtoul(pElem->GetText(),0,10);
        }
    }

    //display
    TiXmlElement* pDisplayElem = pSettingsElem->FirstChildElement("display");
    if(!pDisplayElem)
    {
        cerr << "No 'display' element  !" << endl;
    }
    else
    {
        //positions
        pElem = pDisplayElem->FirstChildElement("positions");
        if(!pElem)
        {
            cerr << "No 'positions' balise !" << endl;
        }
        else
        {
            if(pElem->QueryDoubleAttribute("vRelTeamTop",&m_tPositions.vRelTeamTop) != TIXML_SUCCESS)     cerr << "No 'vRelTeamTop' attribute in positions element !" << endl;
            if(pElem->QueryDoubleAttribute("vRelInterTeam",&m_tPositions.vRelInterTeam) != TIXML_SUCCESS) cerr << "No 'vRelInterTeam' attribute in positions element !" << endl;
            if(pElem->QueryDoubleAttribute("vRelLogoTop",&m_tPositions.vRelLogoTop) != TIXML_SUCCESS)     cerr << "No 'vRelLogoTop' attribute in positions element !" << endl;
            if(pElem->QueryDoubleAttribute("vRelBottom",&m_tPositions.vRelBottom) != TIXML_SUCCESS)       cerr << "No 'vRelBottom' attribute in positions element !" << endl;
            if(pElem->QueryDoubleAttribute("vRelAdMessage",&m_tPositions.vRelAdMessage) != TIXML_SUCCESS) cerr << "No 'vRelAdMessage' attribute in positions element !" << endl;
            if(pElem->QueryDoubleAttribute("hRelLogoRight",&m_tPositions.hRelLogoRight) != TIXML_SUCCESS) cerr << "No 'hRelLogoRight' attribute in positions element !" << endl;

            if(m_tPositions.vRelTeamTop < 0.0)
                m_tPositions.vRelTeamTop = 0;
            if(m_tPositions.vRelInterTeam < 0.0)
                m_tPositions.vRelInterTeam = 0;
            if(m_tPositions.vRelLogoTop < 0.0)
                m_tPositions.vRelLogoTop = 0;
            if(m_tPositions.vRelBottom < 0.0)
                m_tPositions.vRelBottom = 0;
            if(m_tPositions.vRelAdMessage < 0.0)
                m_tPositions.vRelAdMessage = 0;
            if(m_tPositions.hRelLogoRight < 0.0)
                m_tPositions.hRelLogoRight = 0;
        }

        //generation
        pElem = pDisplayElem->FirstChildElement("generation");
        if(!pElem)
        {
            cerr << "No 'generation' balise !" << endl;
        }
        else
        {
            if(pElem->QueryUnsignedAttribute("hMargin",&m_tGeneration.hMargin) != TIXML_SUCCESS)          cerr << "No 'hMargin' attribute in generation element !" << endl;
            if(pElem->QueryUnsignedAttribute("vMargin",&m_tGeneration.vMargin) != TIXML_SUCCESS)          cerr << "No 'vMargin' attribute in generation element !" << endl;
            if(pElem->QueryUnsignedAttribute("hTeamMargin",&m_tGeneration.hTeamMargin) != TIXML_SUCCESS)  cerr << "No 'hTeamMargin' attribute in generation element !" << endl;
            if(pElem->QueryUnsignedAttribute("vTeamMargin",&m_tGeneration.vTeamMargin) != TIXML_SUCCESS)  cerr << "No 'vTeamMargin' attribute in generation element !" << endl;
            if(pElem->QueryUnsignedAttribute("radius",&m_tGeneration.radius) != TIXML_SUCCESS)            cerr << "No 'radius' attribute in generation element !" << endl;
            if(pElem->QueryUnsignedAttribute("smooth",&m_tGeneration.smooth) != TIXML_SUCCESS)            cerr << "No 'smooth' attribute in generation element !" << endl;
            if(pElem->QueryUnsignedAttribute("textDpi",&m_tGeneration.textDpi) != TIXML_SUCCESS)          cerr << "No 'textDpi' attribute in generation element !" << endl;
            if(pElem->QueryDoubleAttribute("logoZoom",&m_tGeneration.logoZoom) != TIXML_SUCCESS)          cerr << "No 'logoZoom' attribute in generation element !" << endl;
            if(pElem->QueryDoubleAttribute("teamZoom",&m_tGeneration.teamZoom) != TIXML_SUCCESS)          cerr << "No 'teamZoom' attribute in generation element !" << endl;
            if(pElem->QueryDoubleAttribute("adZoom",&m_tGeneration.adZoom) != TIXML_SUCCESS)              cerr << "No 'adZoom' attribute in generation element !" << endl;

            if(m_tGeneration.radius < m_tGeneration.smooth)
                m_tGeneration.smooth = m_tGeneration.radius;

            if((m_tGeneration.smooth % 2) == 0)
                m_tGeneration.smooth += 1;
            if(m_tGeneration.smooth >=1000)
                m_tGeneration.smooth = 1;

            if((m_tGeneration.radius % 2) == 0)
                m_tGeneration.radius += 1;

            if(m_tGeneration.radius < m_tGeneration.smooth)
                m_tGeneration.smooth = m_tGeneration.radius;

            if(m_tGeneration.logoZoom <= 0)
                m_tGeneration.logoZoom = 1.0;
            if(m_tGeneration.teamZoom <= 0)
                m_tGeneration.teamZoom = 1.0;
            if(m_tGeneration.adZoom <= 0)
                m_tGeneration.adZoom = 1.0;
        }
    }

    //<teams>
    pElem = hSettings.FirstChildElement("teams").FirstChildElement("team").ToElement();
    for( ; pElem; pElem=pElem->NextSiblingElement("team"))
    {
        Team tTeam;
        int iId = 1;
        if(pElem->QueryIntAttribute("id",&iId) != TIXML_SUCCESS)
        {
            cerr << "No 'id' attribute in team element !" << endl;
        }

        if(iId < 1)
            iId = 1;

        //<name>
        TiXmlElement* pTmpElem = pElem->FirstChildElement("name");
        if(!pTmpElem || !pTmpElem->GetText())
        {
            cerr << "No 'name' element in team element !" << endl;
        }
        else
        {
            tTeam.sName = string(pTmpElem->GetText());
        }

        //<logo>
        pTmpElem = pElem->FirstChildElement("logo");
        if(!pTmpElem || !pTmpElem->GetText())
        {
            cerr << "No 'logo' element in team element !" << endl;
        }
        else
        {
            tTeam.sLogoPath = getOKFilePath(pTmpElem->GetText());
        }

        //<score>
        pTmpElem = pElem->FirstChildElement("score");
        if(!pTmpElem || !pTmpElem->GetText())
        {
            cerr << "No 'score' element in team element !" << endl;
        }
        else
        {
            tTeam.sScore = string(pTmpElem->GetText());
        }

        m_mTeams[iId] = tTeam;
    }

    return true;
}

std::string CSettingsReader::getOKFilePath(const std::string &sFilePath)
{
    boost::filesystem::path path(sFilePath);
    if(path.is_relative())
    {
        path = boost::filesystem::current_path() / path;
    }
//    cerr<<path.string()<<endl;
    return path.string();
}

void CSettingsReader::dumpSettings()
{
    cerr << "Settings file : "  << m_sSettingsFile << endl;
    cerr << "Message : "        << m_sMessage << endl;
    cerr << "Logo : "           << m_sLogoPath << endl;
    cerr << "Ad : "             << m_sAdPath << endl;

    TeamMap::const_iterator itMap;
    for(itMap = m_mTeams.begin(); itMap != m_mTeams.end(); itMap++)
    {
        cerr << "Team "         << itMap->first << endl;
        cerr << "\tName : "     << itMap->second.sName << endl;
        cerr << "\tLogo : "     << itMap->second.sLogoPath << endl;
        cerr << "\tScore : "    << itMap->second.sScore << endl;
    }
}
