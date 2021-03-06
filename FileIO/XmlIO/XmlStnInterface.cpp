/**
 * \file XmlStnInterface.cpp
 * 2011/11/23 KR as derived class from XMLInterface
 */

#include "XmlStnInterface.h"
#include "DateTools.h"
#include "SensorData.h"
#include "StringTools.h"
#include <limits>

#include <QFile>
#include <QtXml/QDomDocument>

namespace FileIO
{

XmlStnInterface::XmlStnInterface(ProjectData* project, const std::string &schemaFile)
: XMLInterface(project, schemaFile)
{
}

int XmlStnInterface::readFile(const QString &fileName)
{
	GEOLIB::GEOObjects* geoObjects = _project->getGEOObjects();
	QFile* file = new QFile(fileName);
	if (!file->open(QIODevice::ReadOnly | QIODevice::Text))
	{
		std::cout << "XmlStnInterface::readFile() - Can't open xml-file." << "\n";
		delete file;
		return 0;
	}
	if (!checkHash(fileName))
	{
		delete file;
		return 0;
	}

	QDomDocument doc("OGS-STN-DOM");
	doc.setContent(file);
	QDomElement docElement = doc.documentElement(); //root element, used for identifying file-type
	if (docElement.nodeName().compare("OpenGeoSysSTN"))
	{
		std::cout << "XmlStnInterface::readFile() - Unexpected XML root." << "\n";
		delete file;
		return 0;
	}

	QDomNodeList lists = docElement.childNodes();
	for (int i = 0; i < lists.count(); i++)
	{
		// read all the station lists
		QDomNodeList stationList = lists.at(i).childNodes();
		std::vector<GEOLIB::Point*>* stations = new std::vector<GEOLIB::Point*>;
		std::string stnName("[NN]");

		for (int j = 0; j < stationList.count(); j++)
		{
			const QDomNode station_node(stationList.at(j));
			const QString station_type(station_node.nodeName());
			if (station_type.compare("name") == 0)
				stnName = station_node.toElement().text().toStdString();
			else if (station_type.compare("stations") == 0)
				this->readStations(station_node, stations, fileName.toStdString());
			else if (station_type.compare("boreholes") == 0)
				this->readStations(station_node, stations, fileName.toStdString());
		}

		if (!stations->empty())
			geoObjects->addStationVec(stations, stnName);
		else
			delete stations;
	}

	delete file;

	return 1;
}

void XmlStnInterface::readStations( const QDomNode &stationsRoot,
                                    std::vector<GEOLIB::Point*>* stations,
								    const std::string &file_name )
{
	QDomElement station = stationsRoot.firstChildElement();
	while (!station.isNull())
	{
		if (station.hasAttribute("id") && station.hasAttribute("x") &&
		    station.hasAttribute("y"))
		{
			std::string stationName("[NN]");
			std::string sensor_data_file_name("");
			std::string boreholeDate("0000-00-00");
			double boreholeDepth(0.0), stationValue(0.0);

			QDomNodeList stationFeatures = station.childNodes();
			for(int i = 0; i < stationFeatures.count(); i++)
			{
				// check for general station features
				const QDomNode feature_node (stationFeatures.at(i));
				const QString feature_name (feature_node.nodeName());
				const std::string element_text (feature_node.toElement().text().toStdString());
				if (feature_name.compare("name") == 0)
					stationName = feature_node.toElement().text().toStdString();
				if (feature_name.compare("sensordata") == 0)
					sensor_data_file_name = feature_node.toElement().text().toStdString();
				/* add other station features here */

				// check for general borehole features
				else if (feature_name.compare("value") == 0)
					stationValue = strtod(element_text.c_str(), 0);
				else if (feature_name.compare("bdepth") == 0)
					boreholeDepth = strtod(element_text.c_str(), 0);
				else if (feature_name.compare("bdate") == 0)
					boreholeDate  = element_text;
				/* add other borehole features here */
			}

			double zVal = (station.hasAttribute("z")) ? strtod((station.attribute("z")).toStdString().c_str(), 0) : 0.0;

			if (station.nodeName().compare("station") == 0)
			{
				GEOLIB::Station* s =
				        new GEOLIB::Station(
							strtod((station.attribute("x")).toStdString().c_str(), 0),
				            strtod((station.attribute("y")).toStdString().c_str(), 0),
				            zVal, 
							stationName);
				s->setStationValue(stationValue);
				if (!sensor_data_file_name.empty())
						s->addSensorDataFromCSV(BaseLib::copyPathToFileName(sensor_data_file_name, file_name));
				stations->push_back(s);
			}
			else if (station.nodeName().compare("borehole") == 0)
			{
				GEOLIB::StationBorehole* s = GEOLIB::StationBorehole::createStation(
				        stationName,
				        strtod((station.attribute("x")).toStdString().c_str(), 0),
				        strtod((station.attribute("y")).toStdString().c_str(), 0),
				        zVal,
				        boreholeDepth,
				        boreholeDate);
				s->setStationValue(stationValue);
				/* add stratigraphy to the borehole */
				for(int j = 0; j < stationFeatures.count(); j++)
					if (stationFeatures.at(j).nodeName().compare("strat") == 0)
						this->readStratigraphy(stationFeatures.at(j), s);

				stations->push_back(s);
			}
		}
		else
			std::cout <<
			"XmlStnInterface::readStations() - Attribute missing in <station> tag ..." <<
			"\n";
		station = station.nextSiblingElement();
	}
}

void XmlStnInterface::readStratigraphy( const QDomNode &stratRoot, GEOLIB::StationBorehole* borehole )
{
	//borehole->addSoilLayer((*borehole)[0], (*borehole)[1], (*borehole)[2], "");
	double depth_check((*borehole)[2]);
	QDomElement horizon = stratRoot.firstChildElement();
	while (!horizon.isNull())
	{
		if (horizon.hasAttribute("id") && horizon.hasAttribute("x") &&
		    horizon.hasAttribute("y") && horizon.hasAttribute("z"))
		{
			std::string horizonName("[NN]");

			QDomNodeList horizonFeatures = horizon.childNodes();
			for(int i = 0; i < horizonFeatures.count(); i++)
				if (horizonFeatures.at(i).nodeName().compare("name") == 0)
					horizonName = horizonFeatures.at(i).toElement().text().toStdString();
				/* add other horizon features here */

			double depth (strtod((horizon.attribute("z")).toStdString().c_str(), 0));
			if (fabs(depth - depth_check) > std::numeric_limits<double>::epsilon()) // skip soil-layer if its thickness is zero
			{
				borehole->addSoilLayer(strtod((horizon.attribute("x")).toStdString().c_str(), 0),
									   strtod((horizon.attribute("y")).toStdString().c_str(), 0),
									   depth,
									   horizonName);
				depth_check = depth;
			}
			else
				std::cout << "Warning: Skipped layer \"" << horizonName << "\" in borehole \"" 
					      << borehole->getName() << "\" because of thickness 0.0." << "\n";
		}
		else
			std::cout <<
			"XmlStnInterface::readStratigraphy() - Attribute missing in <horizon> tag ..."
			          << "\n";
		horizon = horizon.nextSiblingElement();
	}
}

int XmlStnInterface::write(std::ostream& stream)
{
	if (this->_exportName.empty())
	{
		std::cout << "Error in XmlStnInterface::write() - No station list specified..." << "\n";
		return 0;
	}

	GEOLIB::GEOObjects* geoObjects = _project->getGEOObjects();

	stream << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n"; // xml definition
	stream << "<?xml-stylesheet type=\"text/xsl\" href=\"OpenGeoSysSTN.xsl\"?>\n\n"; // stylefile definition

	QDomDocument doc("OGS-STN-DOM");
	QDomElement root = doc.createElement("OpenGeoSysSTN");
	root.setAttribute( "xmlns:ogs", "http://www.opengeosys.net" );
	root.setAttribute( "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance" );
	root.setAttribute( "xsi:noNamespaceSchemaLocation", "http://141.65.34.25/OpenGeoSysSTN.xsd" );

	const std::vector<GEOLIB::Point*>* stations (geoObjects->getStationVec(_exportName));
	bool isBorehole =
	        (static_cast<GEOLIB::Station*>((*stations)[0])->type() ==
	         GEOLIB::Station::BOREHOLE) ? true : false;

	doc.appendChild(root);
	QDomElement stationListTag = doc.createElement("stationlist");
	root.appendChild(stationListTag);

	QDomElement listNameTag = doc.createElement("name");
	stationListTag.appendChild(listNameTag);
	QDomText stationListNameText = doc.createTextNode(QString::fromStdString(_exportName));
	listNameTag.appendChild(stationListNameText);
	QString listType = (isBorehole) ? "boreholes" : "stations";
	QDomElement stationsTag = doc.createElement(listType);
	stationListTag.appendChild(stationsTag);

	bool useStationValue(false);
	double sValue=static_cast<GEOLIB::Station*>((*stations)[0])->getStationValue();
	size_t nStations(stations->size());
	for (size_t i = 1; i < nStations; i++)
		if ((static_cast<GEOLIB::Station*>((*stations)[i])->getStationValue() - sValue) < std::numeric_limits<double>::min())
		{
			useStationValue = true;
			break;
		}

	for (size_t i = 0; i < nStations; i++)
	{
		QString stationType =  (isBorehole) ? "borehole" : "station";
		QDomElement stationTag = doc.createElement(stationType);
		stationTag.setAttribute( "id", QString::number(i) );
		stationTag.setAttribute( "x",  QString::number((*(*stations)[i])[0], 'f') );
		stationTag.setAttribute( "y",  QString::number((*(*stations)[i])[1], 'f') );
		stationTag.setAttribute( "z",  QString::number((*(*stations)[i])[2], 'f') );
		stationsTag.appendChild(stationTag);

		QDomElement stationNameTag = doc.createElement("name");
		stationTag.appendChild(stationNameTag);
		QDomText stationNameText =
		        doc.createTextNode(QString::fromStdString(static_cast<GEOLIB::Station*>((*stations)[i])->getName()));
		stationNameTag.appendChild(stationNameText);

		if (useStationValue)
		{
			QDomElement stationValueTag = doc.createElement("value");
			stationTag.appendChild(stationValueTag);
			QDomText stationValueText =
					doc.createTextNode(QString::number(static_cast<GEOLIB::Station*>((*stations)[i])->getStationValue()));
			stationValueTag.appendChild(stationValueText);
		}

		if (isBorehole)
			writeBoreholeData(doc, stationTag,
			                  static_cast<GEOLIB::StationBorehole*>((*stations)[i]));
	}

	std::string xml = doc.toString().toStdString();
	stream << xml;
	return 1;
}

void XmlStnInterface::writeBoreholeData(QDomDocument &doc,
                                     QDomElement &boreholeTag,
                                     GEOLIB::StationBorehole* borehole) const
{
	QDomElement stationDepthTag = doc.createElement("bdepth");
	boreholeTag.appendChild(stationDepthTag);
	QDomText stationDepthText = doc.createTextNode(QString::number(borehole->getDepth(), 'f'));
	stationDepthTag.appendChild(stationDepthText);
	if (fabs(borehole->getDate()) > 0)
	{
		QDomElement stationDateTag = doc.createElement("bdate");
		boreholeTag.appendChild(stationDateTag);
		QDomText stationDateText =
		        doc.createTextNode(QString::fromStdString(date2string(borehole->getDate())));
		stationDateTag.appendChild(stationDateText);
	}

	std::vector<GEOLIB::Point*> profile = borehole->getProfile();
	std::vector<std::string> soilNames = borehole->getSoilNames();
	size_t nHorizons(profile.size());

	if (nHorizons > 1)
	{
		QDomElement stratTag = doc.createElement("strat");
		boreholeTag.appendChild(stratTag);

		for (size_t j = 1; j < nHorizons; j++) /// the first entry in the profile vector is just the position of the borehole
		{
			QDomElement horizonTag = doc.createElement("horizon");
			horizonTag.setAttribute( "id", QString::number(j) );
			horizonTag.setAttribute( "x",  QString::number((*profile[j])[0], 'f') );
			horizonTag.setAttribute( "y",  QString::number((*profile[j])[1], 'f') );
			horizonTag.setAttribute( "z",  QString::number((*profile[j])[2], 'f') );
			stratTag.appendChild(horizonTag);
			QDomElement horizonNameTag = doc.createElement("name");
			horizonTag.appendChild(horizonNameTag);
			QDomText horizonNameText =
			        doc.createTextNode(QString::fromStdString(soilNames[j]));
			horizonNameTag.appendChild(horizonNameText);
		}
	}
}

}
