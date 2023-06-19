import xml.etree.ElementTree as ET

# Path to the XML file
xml_file_path = "bag/Leveringsdocument-BAG-Extract.xml"

# Parse the XML file
tree = ET.parse(xml_file_path)
root = tree.getroot()

# Find the desired element and extract its value
namespace = {'selecties-extract': 'http://www.kadaster.nl/schemas/lvbag/extract-selecties/v20200601'}
stand_technische_datum = root.find('.//selecties-extract:StandTechnischeDatum', namespace).text

print(stand_technische_datum)


