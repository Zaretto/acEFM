<?xml version="1.0" encoding="UTF-8"?>
<!--
  autotest-report-fo.xslt
  Transforms <autotest-report> XML into XSL-FO for Apache FOP rendering.
  Produces A4 portrait PDF with aviation-style document headers.
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:fo="http://www.w3.org/1999/XSL/Format"
    xmlns:at="urn:aviastorm:autotest-results:1.0">

  <xsl:output method="xml" indent="yes"/>

  <!-- Colour constants -->
  <xsl:variable name="navy">#1B3A5C</xsl:variable>
  <xsl:variable name="light-navy">#2D5F8A</xsl:variable>
  <xsl:variable name="green">#1A7A1A</xsl:variable>
  <xsl:variable name="red">#CC0000</xsl:variable>
  <xsl:variable name="light-grey">#F0F0F0</xsl:variable>
  <xsl:variable name="mid-grey">#E0E0E0</xsl:variable>

  <!-- ============================================================ -->
  <!-- Root template                                                 -->
  <!-- ============================================================ -->
  <xsl:template match="/at:autotest-report">
    <fo:root>
      <fo:layout-master-set>
        <fo:simple-page-master master-name="A4-portrait"
            page-width="210mm" page-height="297mm"
            margin-top="15mm" margin-bottom="15mm"
            margin-left="15mm" margin-right="15mm">
          <fo:region-body margin-top="12mm" margin-bottom="12mm"/>
          <fo:region-before extent="10mm"/>
          <fo:region-after extent="10mm"/>
        </fo:simple-page-master>
      </fo:layout-master-set>

      <fo:page-sequence master-reference="A4-portrait"
                        font-family="Helvetica, Arial, sans-serif"
                        font-size="8pt">
        <!-- Header -->
        <fo:static-content flow-name="xsl-region-before">
          <fo:block font-size="7pt" color="#666666">
            <fo:inline>
              <xsl:value-of select="at:metadata/at:document-number"/>
            </fo:inline>
            <fo:leader leader-pattern="space"/>
            <fo:inline font-style="italic">
              <xsl:value-of select="at:metadata/at:classification"/>
            </fo:inline>
          </fo:block>
        </fo:static-content>

        <!-- Footer -->
        <fo:static-content flow-name="xsl-region-after">
          <fo:block text-align="center" font-size="7pt" color="#666666">
            Page <fo:page-number/> of <fo:page-number-citation ref-id="last-page"/>
          </fo:block>
        </fo:static-content>

        <!-- Body -->
        <fo:flow flow-name="xsl-region-body">
          <!-- Title banner -->
          <xsl:choose>
            <xsl:when test="at:metadata/at:company or at:metadata/at:logo">
              <fo:table table-layout="fixed" width="100%" background-color="{$navy}"
                        margin-bottom="4mm">
                <xsl:choose>
                  <xsl:when test="at:metadata/at:logo">
                    <fo:table-column column-width="20mm"/>
                    <fo:table-column column-width="proportional-column-width(1)"/>
                  </xsl:when>
                  <xsl:otherwise>
                    <fo:table-column column-width="proportional-column-width(1)"/>
                  </xsl:otherwise>
                </xsl:choose>
                <fo:table-body>
                  <fo:table-row>
                    <xsl:if test="at:metadata/at:logo">
                      <fo:table-cell number-rows-spanned="2" padding="4pt"
                                     display-align="center">
                        <fo:block>
                          <fo:external-graphic content-height="12mm" scaling="uniform">
                            <xsl:attribute name="src">
                              <xsl:value-of select="at:metadata/at:logo"/>
                            </xsl:attribute>
                          </fo:external-graphic>
                        </fo:block>
                      </fo:table-cell>
                    </xsl:if>
                    <fo:table-cell padding="6pt 10pt 2pt">
                      <fo:block color="white" font-size="11pt" font-weight="bold"
                                letter-spacing="0.5pt">
                        <xsl:value-of select="at:metadata/at:company"/>
                      </fo:block>
                    </fo:table-cell>
                  </fo:table-row>
                  <fo:table-row>
                    <fo:table-cell padding="2pt 10pt 6pt">
                      <fo:block color="white" font-size="14pt" font-weight="bold"
                                letter-spacing="0.5pt">
                        <xsl:text>AUTOTEST REPORT</xsl:text>
                      </fo:block>
                    </fo:table-cell>
                  </fo:table-row>
                </fo:table-body>
              </fo:table>
            </xsl:when>
            <xsl:otherwise>
              <fo:block background-color="{$navy}" color="white"
                        font-size="14pt" font-weight="bold"
                        padding="6pt 10pt" margin-bottom="4mm"
                        text-align="center">
                <xsl:text>AUTOTEST REPORT</xsl:text>
              </fo:block>
            </xsl:otherwise>
          </xsl:choose>

          <!-- Document control table -->
          <fo:table table-layout="fixed" width="100%" border="0.5pt solid {$navy}"
                    margin-bottom="3mm">
            <fo:table-column column-width="30%"/>
            <fo:table-column column-width="45%"/>
            <fo:table-column column-width="25%"/>
            <fo:table-header>
              <fo:table-row background-color="{$navy}" color="white">
                <fo:table-cell padding="3pt"><fo:block font-weight="bold">DOCUMENT NO.</fo:block></fo:table-cell>
                <fo:table-cell padding="3pt"><fo:block font-weight="bold">TITLE</fo:block></fo:table-cell>
                <fo:table-cell padding="3pt"><fo:block font-weight="bold">DATE</fo:block></fo:table-cell>
              </fo:table-row>
            </fo:table-header>
            <fo:table-body>
              <fo:table-row>
                <fo:table-cell padding="3pt" border="0.5pt solid {$mid-grey}">
                  <fo:block><xsl:value-of select="at:metadata/at:document-number"/></fo:block>
                </fo:table-cell>
                <fo:table-cell padding="3pt" border="0.5pt solid {$mid-grey}">
                  <fo:block><xsl:value-of select="at:metadata/at:title"/></fo:block>
                </fo:table-cell>
                <fo:table-cell padding="3pt" border="0.5pt solid {$mid-grey}">
                  <fo:block><xsl:value-of select="at:metadata/at:date"/></fo:block>
                </fo:table-cell>
              </fo:table-row>
            </fo:table-body>
          </fo:table>

          <!-- Metadata rows -->
          <fo:table table-layout="fixed" width="100%" margin-bottom="4mm">
            <fo:table-column column-width="25%"/>
            <fo:table-column column-width="75%"/>
            <fo:table-body>
              <xsl:if test="at:metadata/at:aircraft != ''">
                <fo:table-row>
                  <fo:table-cell padding="2pt" background-color="{$light-grey}">
                    <fo:block font-weight="bold">AIRCRAFT</fo:block>
                  </fo:table-cell>
                  <fo:table-cell padding="2pt">
                    <fo:block><xsl:value-of select="at:metadata/at:aircraft"/></fo:block>
                  </fo:table-cell>
                </fo:table-row>
              </xsl:if>
              <xsl:if test="at:metadata/at:system != ''">
                <fo:table-row>
                  <fo:table-cell padding="2pt" background-color="{$light-grey}">
                    <fo:block font-weight="bold">SYSTEM</fo:block>
                  </fo:table-cell>
                  <fo:table-cell padding="2pt">
                    <fo:block><xsl:value-of select="at:metadata/at:system"/></fo:block>
                  </fo:table-cell>
                </fo:table-row>
              </xsl:if>
              <xsl:if test="at:metadata/at:ata-chapter != ''">
                <fo:table-row>
                  <fo:table-cell padding="2pt" background-color="{$light-grey}">
                    <fo:block font-weight="bold">ATA CHAPTER</fo:block>
                  </fo:table-cell>
                  <fo:table-cell padding="2pt">
                    <fo:block><xsl:value-of select="at:metadata/at:ata-chapter"/></fo:block>
                  </fo:table-cell>
                </fo:table-row>
              </xsl:if>
              <xsl:if test="at:metadata/at:reference != ''">
                <fo:table-row>
                  <fo:table-cell padding="2pt" background-color="{$light-grey}">
                    <fo:block font-weight="bold">REFERENCE</fo:block>
                  </fo:table-cell>
                  <fo:table-cell padding="2pt">
                    <fo:block><xsl:value-of select="at:metadata/at:reference"/></fo:block>
                  </fo:table-cell>
                </fo:table-row>
              </xsl:if>
              <fo:table-row>
                <fo:table-cell padding="2pt" background-color="{$light-grey}">
                  <fo:block font-weight="bold">REVISION</fo:block>
                </fo:table-cell>
                <fo:table-cell padding="2pt">
                  <fo:block><xsl:value-of select="at:metadata/at:revision"/></fo:block>
                </fo:table-cell>
              </fo:table-row>
              <xsl:if test="at:metadata/at:prepared-by != ''">
                <fo:table-row>
                  <fo:table-cell padding="2pt" background-color="{$light-grey}">
                    <fo:block font-weight="bold">PREPARED BY</fo:block>
                  </fo:table-cell>
                  <fo:table-cell padding="2pt">
                    <fo:block><xsl:value-of select="at:metadata/at:prepared-by"/></fo:block>
                  </fo:table-cell>
                </fo:table-row>
              </xsl:if>
              <fo:table-row>
                <fo:table-cell padding="2pt" background-color="{$light-grey}">
                  <fo:block font-weight="bold">CLASSIFICATION</fo:block>
                </fo:table-cell>
                <fo:table-cell padding="2pt">
                  <fo:block><xsl:value-of select="at:metadata/at:classification"/></fo:block>
                </fo:table-cell>
              </fo:table-row>
            </fo:table-body>
          </fo:table>

          <!-- Summary -->
          <fo:block font-size="10pt" font-weight="bold" color="{$navy}"
                    margin-bottom="2mm" margin-top="2mm">
            Test Summary
          </fo:block>
          <fo:table table-layout="fixed" width="50%" border="0.5pt solid {$navy}"
                    margin-bottom="4mm">
            <fo:table-column column-width="25%"/>
            <fo:table-column column-width="25%"/>
            <fo:table-column column-width="25%"/>
            <fo:table-column column-width="25%"/>
            <fo:table-header>
              <fo:table-row background-color="{$navy}" color="white">
                <fo:table-cell padding="3pt"><fo:block font-weight="bold">Passed</fo:block></fo:table-cell>
                <fo:table-cell padding="3pt"><fo:block font-weight="bold">Failed</fo:block></fo:table-cell>
                <fo:table-cell padding="3pt"><fo:block font-weight="bold">Errors</fo:block></fo:table-cell>
                <fo:table-cell padding="3pt"><fo:block font-weight="bold">Total</fo:block></fo:table-cell>
              </fo:table-row>
            </fo:table-header>
            <fo:table-body>
              <fo:table-row>
                <fo:table-cell padding="3pt"><fo:block color="{$green}" font-weight="bold"><xsl:value-of select="at:summary/@passed"/></fo:block></fo:table-cell>
                <fo:table-cell padding="3pt">
                  <fo:block font-weight="bold">
                    <xsl:attribute name="color">
                      <xsl:choose>
                        <xsl:when test="at:summary/@failed &gt; 0"><xsl:value-of select="$red"/></xsl:when>
                        <xsl:otherwise><xsl:value-of select="$green"/></xsl:otherwise>
                      </xsl:choose>
                    </xsl:attribute>
                    <xsl:value-of select="at:summary/@failed"/>
                  </fo:block>
                </fo:table-cell>
                <fo:table-cell padding="3pt">
                  <fo:block font-weight="bold">
                    <xsl:attribute name="color">
                      <xsl:choose>
                        <xsl:when test="at:summary/@errors &gt; 0"><xsl:value-of select="$red"/></xsl:when>
                        <xsl:otherwise>#333333</xsl:otherwise>
                      </xsl:choose>
                    </xsl:attribute>
                    <xsl:value-of select="at:summary/@errors"/>
                  </fo:block>
                </fo:table-cell>
                <fo:table-cell padding="3pt"><fo:block font-weight="bold"><xsl:value-of select="at:summary/@total"/></fo:block></fo:table-cell>
              </fo:table-row>
            </fo:table-body>
          </fo:table>

          <!-- Per-test sections -->
          <xsl:apply-templates select="at:test"/>

          <fo:block id="last-page"/>
        </fo:flow>
      </fo:page-sequence>
    </fo:root>
  </xsl:template>

  <!-- ============================================================ -->
  <!-- Test section template                                         -->
  <!-- ============================================================ -->
  <xsl:template match="at:test">
    <!-- Test header block — keep heading, description, and status together -->
    <fo:block keep-together.within-page="always"
              keep-with-next.within-page="always"
              margin-top="6mm">

      <!-- Test heading -->
      <fo:block font-size="11pt" font-weight="bold" color="{$navy}"
                border-bottom="1pt solid {$navy}" padding-bottom="2pt"
                margin-bottom="2mm">
        <xsl:value-of select="@name"/>
      </fo:block>

      <!-- Description -->
      <xsl:if test="at:description != ''">
        <fo:block font-style="italic" margin-bottom="2mm">
          <xsl:value-of select="at:description"/>
        </fo:block>
      </xsl:if>

      <!-- Result status -->
      <fo:block margin-bottom="3mm" font-size="9pt">
      <fo:inline font-weight="bold">
        <xsl:attribute name="color">
          <xsl:choose>
            <xsl:when test="at:result/@status = 'pass'"><xsl:value-of select="$green"/></xsl:when>
            <xsl:when test="at:result/@status = 'fail'"><xsl:value-of select="$red"/></xsl:when>
            <xsl:otherwise><xsl:value-of select="$red"/></xsl:otherwise>
          </xsl:choose>
        </xsl:attribute>
        <xsl:value-of select="translate(at:result/@status,
          'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')"/>
      </fo:inline>
      <xsl:if test="at:result/@properties-total">
        <xsl:text> — </xsl:text>
        <xsl:value-of select="at:result/@properties-passed"/>
        <xsl:text>/</xsl:text>
        <xsl:value-of select="at:result/@properties-total"/>
        <xsl:text> properties within tolerance</xsl:text>
      </xsl:if>
      <xsl:if test="at:result/@checks-total">
        <xsl:text>, </xsl:text>
        <xsl:value-of select="at:result/@checks-passed"/>
        <xsl:text>/</xsl:text>
        <xsl:value-of select="at:result/@checks-total"/>
        <xsl:text> checks passed</xsl:text>
      </xsl:if>
      <xsl:if test="at:result/@check-events-expected">
        <xsl:text>, </xsl:text>
        <xsl:value-of select="at:result/@check-events-fired"/>
        <xsl:text>/</xsl:text>
        <xsl:value-of select="at:result/@check-events-expected"/>
        <xsl:text> check events fired</xsl:text>
      </xsl:if>
    </fo:block>

    <!-- Error message -->
    <xsl:if test="at:result/at:message">
      <fo:block color="{$red}" margin-bottom="3mm">
        <xsl:value-of select="at:result/at:message"/>
      </fo:block>
    </xsl:if>

    </fo:block><!-- end keep-together header block -->

    <!-- Property results table (all properties) -->
    <xsl:if test="at:property-results/at:property">
      <fo:block font-size="9pt" font-weight="bold" color="{$navy}"
                margin-bottom="1mm">
        Property Results
      </fo:block>
      <fo:table table-layout="fixed" width="100%" border="0.5pt solid #999999"
                margin-bottom="3mm">
        <fo:table-column column-width="35%"/>
        <fo:table-column column-width="13%"/>
        <fo:table-column column-width="13%"/>
        <fo:table-column column-width="13%"/>
        <fo:table-column column-width="13%"/>
        <fo:table-column column-width="13%"/>
        <fo:table-header>
          <fo:table-row background-color="{$navy}" color="white">
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Property</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Max Delta</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Tol (abs)</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Tol (rel)</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Baseline</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Result</fo:block></fo:table-cell>
          </fo:table-row>
        </fo:table-header>
        <fo:table-body>
          <xsl:for-each select="at:property-results/at:property">
            <fo:table-row>
              <xsl:if test="position() mod 2 = 0">
                <xsl:attribute name="background-color"><xsl:value-of select="$light-grey"/></xsl:attribute>
              </xsl:if>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block font-size="7pt"><xsl:value-of select="@name"/></fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block><xsl:value-of select="@max-delta"/></fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block>
                  <xsl:choose>
                    <xsl:when test="@tol-abs"><xsl:value-of select="@tol-abs"/></xsl:when>
                    <xsl:otherwise>—</xsl:otherwise>
                  </xsl:choose>
                </fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block>
                  <xsl:choose>
                    <xsl:when test="@tol-rel"><xsl:value-of select="@tol-rel"/></xsl:when>
                    <xsl:otherwise>—</xsl:otherwise>
                  </xsl:choose>
                </fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block>
                  <xsl:choose>
                    <xsl:when test="@baseline-at-max"><xsl:value-of select="@baseline-at-max"/></xsl:when>
                    <xsl:otherwise>—</xsl:otherwise>
                  </xsl:choose>
                </fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block font-weight="bold">
                  <xsl:attribute name="color">
                    <xsl:choose>
                      <xsl:when test="@passed = 'true'"><xsl:value-of select="$green"/></xsl:when>
                      <xsl:otherwise><xsl:value-of select="$red"/></xsl:otherwise>
                    </xsl:choose>
                  </xsl:attribute>
                  <xsl:choose>
                    <xsl:when test="@passed = 'true'">PASS</xsl:when>
                    <xsl:otherwise>FAIL</xsl:otherwise>
                  </xsl:choose>
                </fo:block>
              </fo:table-cell>
            </fo:table-row>
          </xsl:for-each>
        </fo:table-body>
      </fo:table>
    </xsl:if>

    <!-- Check Events table: each event carrying checks must have fired -->
    <xsl:if test="at:check-events/at:check-event">
      <fo:block font-size="9pt" font-weight="bold" color="{$navy}"
                margin-bottom="1mm">
        Check Events
      </fo:block>
      <fo:table table-layout="fixed" width="100%" border="0.5pt solid #999999"
                margin-bottom="3mm">
        <fo:table-column column-width="40%"/>
        <fo:table-column column-width="10%"/>
        <fo:table-column column-width="10%"/>
        <fo:table-column column-width="12%"/>
        <fo:table-column column-width="28%"/>
        <fo:table-header>
          <fo:table-row background-color="{$navy}" color="white">
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Event</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Checks</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Fired</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Result</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Message</fo:block></fo:table-cell>
          </fo:table-row>
        </fo:table-header>
        <fo:table-body>
          <xsl:for-each select="at:check-events/at:check-event">
            <fo:table-row>
              <xsl:if test="position() mod 2 = 0">
                <xsl:attribute name="background-color"><xsl:value-of select="$light-grey"/></xsl:attribute>
              </xsl:if>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block font-size="7pt"><xsl:value-of select="@name"/></fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block><xsl:value-of select="@checks"/></fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block><xsl:value-of select="@fired"/></fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block font-weight="bold">
                  <xsl:attribute name="color">
                    <xsl:choose>
                      <xsl:when test="@passed = 'true'">#1a7f37</xsl:when>
                      <xsl:otherwise>#c00000</xsl:otherwise>
                    </xsl:choose>
                  </xsl:attribute>
                  <xsl:choose>
                    <xsl:when test="@passed = 'true'">RUN</xsl:when>
                    <xsl:otherwise>NOT RUN</xsl:otherwise>
                  </xsl:choose>
                </fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block font-size="7pt"><xsl:value-of select="@message"/></fo:block>
              </fo:table-cell>
            </fo:table-row>
          </xsl:for-each>
        </fo:table-body>
      </fo:table>
    </xsl:if>

    <!-- Event Checks table -->
    <xsl:if test="at:check-results/at:check">
      <fo:block font-size="9pt" font-weight="bold" color="{$navy}"
                margin-bottom="1mm">
        Event Checks
      </fo:block>
      <fo:table table-layout="fixed" width="100%" border="0.5pt solid #999999"
                margin-bottom="3mm">
        <fo:table-column column-width="25%"/>
        <fo:table-column column-width="10%"/>
        <fo:table-column column-width="12%"/>
        <fo:table-column column-width="12%"/>
        <fo:table-column column-width="10%"/>
        <fo:table-column column-width="8%"/>
        <fo:table-column column-width="23%"/>
        <fo:table-header>
          <fo:table-row background-color="{$navy}" color="white">
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Property</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Time</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Expected</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Actual</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Tolerance</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Result</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Message</fo:block></fo:table-cell>
          </fo:table-row>
        </fo:table-header>
        <fo:table-body>
          <xsl:for-each select="at:check-results/at:check">
            <fo:table-row>
              <xsl:if test="position() mod 2 = 0">
                <xsl:attribute name="background-color"><xsl:value-of select="$light-grey"/></xsl:attribute>
              </xsl:if>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block font-size="7pt"><xsl:value-of select="@property"/></fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block>
                  <xsl:choose>
                    <xsl:when test="@time"><xsl:value-of select="@time"/></xsl:when>
                    <xsl:otherwise>—</xsl:otherwise>
                  </xsl:choose>
                </fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block><xsl:value-of select="@expected"/></fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block><xsl:value-of select="@actual"/></fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block><xsl:value-of select="@tolerance"/></fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block font-weight="bold">
                  <xsl:attribute name="color">
                    <xsl:choose>
                      <xsl:when test="@passed = 'true'"><xsl:value-of select="$green"/></xsl:when>
                      <xsl:otherwise><xsl:value-of select="$red"/></xsl:otherwise>
                    </xsl:choose>
                  </xsl:attribute>
                  <xsl:choose>
                    <xsl:when test="@passed = 'true'">PASS</xsl:when>
                    <xsl:otherwise>FAIL</xsl:otherwise>
                  </xsl:choose>
                </fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block font-size="7pt"><xsl:value-of select="@message"/></fo:block>
              </fo:table-cell>
            </fo:table-row>
          </xsl:for-each>
        </fo:table-body>
      </fo:table>
    </xsl:if>

    <!-- Initial conditions table -->
    <xsl:if test="at:initial-conditions/at:condition">
      <fo:block font-size="9pt" font-weight="bold" color="{$navy}"
                margin-bottom="1mm">
        Initial Conditions
      </fo:block>
      <fo:table table-layout="fixed" width="100%" border="0.5pt solid #999999"
                margin-bottom="3mm">
        <fo:table-column column-width="20%"/>
        <fo:table-column column-width="15%"/>
        <fo:table-column column-width="30%"/>
        <fo:table-column column-width="35%"/>
        <fo:table-header>
          <fo:table-row background-color="{$navy}" color="white">
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Section</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Frame</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Values</fo:block></fo:table-cell>
            <fo:table-cell padding="2pt"><fo:block font-weight="bold">Description</fo:block></fo:table-cell>
          </fo:table-row>
        </fo:table-header>
        <fo:table-body>
          <xsl:for-each select="at:initial-conditions/at:condition">
            <fo:table-row>
              <xsl:if test="position() mod 2 = 0">
                <xsl:attribute name="background-color"><xsl:value-of select="$light-grey"/></xsl:attribute>
              </xsl:if>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block><xsl:value-of select="@section"/></fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block><xsl:value-of select="@frame"/></fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block font-size="7pt"><xsl:value-of select="@values"/></fo:block>
              </fo:table-cell>
              <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                <fo:block font-size="7pt"><xsl:value-of select="@description"/></fo:block>
              </fo:table-cell>
            </fo:table-row>
          </xsl:for-each>
        </fo:table-body>
      </fo:table>
    </xsl:if>

    <!-- Events -->
    <xsl:if test="at:events/at:event">
      <fo:block font-size="9pt" font-weight="bold" color="{$navy}"
                margin-bottom="1mm">
        Events
      </fo:block>
      <xsl:for-each select="at:events/at:event">
        <fo:block font-size="8pt" font-weight="bold" color="{$light-navy}"
                  margin-bottom="1mm" margin-top="1mm">
          <xsl:value-of select="@name"/>
          <xsl:text> (t=</xsl:text>
          <xsl:value-of select="@time"/>
          <xsl:text>s)</xsl:text>
        </fo:block>
        <xsl:if test="at:property">
          <fo:table table-layout="fixed" width="80%" border="0.5pt solid #CCCCCC"
                    margin-bottom="2mm">
            <fo:table-column column-width="50%"/>
            <fo:table-column column-width="50%"/>
            <fo:table-header>
              <fo:table-row background-color="{$light-navy}" color="white">
                <fo:table-cell padding="2pt"><fo:block font-weight="bold">Property</fo:block></fo:table-cell>
                <fo:table-cell padding="2pt"><fo:block font-weight="bold">Value</fo:block></fo:table-cell>
              </fo:table-row>
            </fo:table-header>
            <fo:table-body>
              <xsl:for-each select="at:property">
                <fo:table-row>
                  <xsl:if test="position() mod 2 = 0">
                    <xsl:attribute name="background-color"><xsl:value-of select="$light-grey"/></xsl:attribute>
                  </xsl:if>
                  <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                    <fo:block font-size="7pt"><xsl:value-of select="@name"/></fo:block>
                  </fo:table-cell>
                  <fo:table-cell padding="2pt" border="0.5pt solid {$mid-grey}">
                    <fo:block><xsl:value-of select="@value"/></fo:block>
                  </fo:table-cell>
                </fo:table-row>
              </xsl:for-each>
            </fo:table-body>
          </fo:table>
        </xsl:if>
      </xsl:for-each>
    </xsl:if>

    <!-- Plots — titled, on same page as test if space allows -->
    <xsl:for-each select="at:plots/at:plot">
      <fo:block keep-together.within-page="auto" margin-top="3mm">
        <fo:block font-size="9pt" font-weight="bold" color="{$navy}"
                  margin-bottom="1mm"
                  keep-with-next.within-page="always">
          <xsl:value-of select="../../@name"/>
          <xsl:text> — Overlay Plot</xsl:text>
        </fo:block>
        <fo:block text-align="center">
          <fo:external-graphic
              content-width="180mm"
              content-height="200mm"
              scaling="uniform">
            <xsl:attribute name="src">
              <xsl:value-of select="@file"/>
            </xsl:attribute>
          </fo:external-graphic>
        </fo:block>
      </fo:block>
    </xsl:for-each>
  </xsl:template>

</xsl:stylesheet>
