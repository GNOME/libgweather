<xsl:stylesheet version="1.0"
 xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
 <xsl:output indent="yes" encoding="utf-8" doctype-system="locations.dtd"/>
 <xsl:strip-space elements="*"/>
 <xsl:preserve-space elements="timezone"/>

<!-- TODO:
  - move locations to the top of their container, so
    <state>
      <location/>
      <city/>
    </state>

  - de-duplicate locations
  - don't rewrite <timezone id="Africa/Algiers" /> to
    <timezone id="Africa/Algiers"/>
-->

 <xsl:template match="node()|@*">
     <xsl:copy>
       <xsl:apply-templates select="node()|@*"/>
     </xsl:copy>
 </xsl:template>

 <xsl:template match="city[location]">
  <xsl:apply-templates select="location"/>
  <xsl:copy>
    <xsl:apply-templates select="node()[not(self::location)]|@*"/>
  </xsl:copy>
 </xsl:template>

</xsl:stylesheet>
