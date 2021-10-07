# PNG Strips MultiThreading
Use MultiThreading to combine 50 PNG "Strips" from a website into a proper image<br/><br/>

paster : contains the multi-threading logic and calls other class's methods to combine the PNG Strips <br/>
catpng : contains the methods that break PNG data into struct format and combines them <br/>
crc    : contains the logic for PNG CRC Calculations <br/>
zutil  : contains the logic to inflate PNG data when receiving and deflating the data when combining <br/>
