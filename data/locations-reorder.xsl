<xsl:stylesheet version="1.0"
 xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
 <xsl:output indent="yes" encoding="utf-8" doctype-system="locations.dtd"/>
 <xsl:strip-space elements="*"/>
 <xsl:preserve-space elements="timezone"/>

 <xsl:template match="node()|@*">
     <xsl:copy>
       <xsl:apply-templates select="node()|@*"/>
     </xsl:copy>
 </xsl:template>

 <xsl:template match="country">
	 <xsl:copy>
		 <xsl:apply-templates select="comment()[following-sibling::_name]"/>
		 <xsl:apply-templates select="_name"/>
		 <xsl:apply-templates select="comment()[not(following-sibling::_name)]"/>
		 <xsl:apply-templates select="iso-code"/>
		 <xsl:apply-templates select="fips-code"/>
		 <xsl:apply-templates select="timezones"/>
		 <xsl:apply-templates select="tz-hint"/>
		 <xsl:apply-templates select="location"/>
		 <xsl:apply-templates select="state"/>
		 <xsl:apply-templates select="city"/>
	 </xsl:copy>
 </xsl:template>

 <xsl:template match="state">
	 <xsl:copy>
		 <xsl:apply-templates select="comment()[following-sibling::_name]"/>
		 <xsl:apply-templates select="_name"/>
		 <xsl:apply-templates select="comment()[not(following-sibling::_name)]"/>
		 <xsl:apply-templates select="iso-code"/>
		 <xsl:apply-templates select="fips-code"/>
		 <xsl:apply-templates select="timezones"/>
		 <xsl:apply-templates select="tz-hint"/>
		 <xsl:apply-templates select="location"/>
		 <xsl:apply-templates select="city"/>
	 </xsl:copy>
 </xsl:template>

</xsl:stylesheet>
