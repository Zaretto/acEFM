<?xml version="1.0" encoding="UTF-8"?>
<!--
  autotest-summary-html.xslt
  Transforms <autotest-summary> XML into HTML for browser viewing.
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:at="urn:aviastorm:autotest-results:1.0">

  <xsl:output method="html" indent="yes" encoding="UTF-8"/>

  <xsl:template match="/at:autotest-summary">
    <html>
    <head>
      <meta charset="UTF-8"/>
      <title>Validation Summary — <xsl:value-of select="at:metadata/at:aircraft"/></title>
      <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: -apple-system, 'Segoe UI', Helvetica, Arial, sans-serif;
               font-size: 14px; color: #222; background: #f8f9fa; padding: 20px; }
        .container { max-width: 1000px; margin: 0 auto; }

        .banner { background: #1B3A5C; color: #fff; text-align: center;
                  padding: 12px 20px; font-size: 18px; font-weight: bold;
                  letter-spacing: 1px; margin-bottom: 8px; }
        .banner-table { width: 100%; background: #1B3A5C; color: #fff;
                        margin-bottom: 8px; border-collapse: collapse; }
        .banner-logo { padding: 8px 16px; width: 80px; vertical-align: middle; }
        .banner-logo-img { max-height: 48px; max-width: 64px; }
        .banner-company { padding: 8px 16px; font-size: 16px; font-weight: bold;
                          letter-spacing: 1px; }
        .banner-title { padding: 4px 16px 8px; font-size: 18px; font-weight: bold;
                        letter-spacing: 1px; }
        .subtitle { text-align: center; margin-bottom: 4px; font-size: 16px; }
        .date { text-align: center; color: #666; font-size: 13px; margin-bottom: 16px; }

        .summary-bar { display: flex; gap: 16px; justify-content: center;
                       margin-bottom: 20px; padding: 10px 16px;
                       background: #fff; border: 1px solid #ddd; border-radius: 4px; }
        .summary-bar .stat { font-size: 22px; font-weight: bold; text-align: center; }
        .summary-bar .stat-label { font-size: 11px; color: #666;
                                    text-transform: uppercase; text-align: center; }
        .pass { color: #1A7A1A; }
        .fail { color: #CC0000; }

        .section-title { font-size: 15px; font-weight: bold; color: #1B3A5C;
                         margin: 16px 0 8px; border-bottom: 2px solid #1B3A5C;
                         padding-bottom: 4px; }

        table.data { width: 100%; border-collapse: collapse; margin-bottom: 16px;
                     font-size: 13px; }
        table.data th { background: #1B3A5C; color: #fff; padding: 6px 10px;
                        text-align: left; font-size: 12px; }
        table.data td { padding: 5px 10px; border-bottom: 1px solid #e8e8e8; }
        table.data tr:nth-child(even) { background: #f8f8f8; }
        table.data a { color: #1B3A5C; text-decoration: none; font-weight: bold; }
        table.data a:hover { text-decoration: underline; }
      </style>
    </head>
    <body>
      <div class="container">
        <xsl:choose>
          <xsl:when test="at:metadata/at:company or at:metadata/at:logo">
            <table class="banner-table">
              <tr>
                <xsl:if test="at:metadata/at:logo">
                  <td class="banner-logo" rowspan="2">
                    <img class="banner-logo-img">
                      <xsl:attribute name="src"><xsl:value-of select="at:metadata/at:logo"/></xsl:attribute>
                      <xsl:attribute name="alt">Logo</xsl:attribute>
                    </img>
                  </td>
                </xsl:if>
                <td class="banner-company">
                  <xsl:value-of select="at:metadata/at:company"/>
                </td>
              </tr>
              <tr>
                <td class="banner-title">VALIDATION SUMMARY</td>
              </tr>
            </table>
          </xsl:when>
          <xsl:otherwise>
            <div class="banner">VALIDATION SUMMARY</div>
          </xsl:otherwise>
        </xsl:choose>
        <div class="subtitle"><xsl:value-of select="at:metadata/at:aircraft"/></div>
        <div class="date"><xsl:value-of select="at:metadata/at:date"/></div>

        <div class="summary-bar">
          <div>
            <div class="stat pass"><xsl:value-of select="at:summary/@passed"/></div>
            <div class="stat-label">Passed</div>
          </div>
          <div>
            <div>
              <xsl:attribute name="class">stat <xsl:choose>
                <xsl:when test="at:summary/@failed &gt; 0">fail</xsl:when>
                <xsl:otherwise>pass</xsl:otherwise>
              </xsl:choose></xsl:attribute>
              <xsl:value-of select="at:summary/@failed"/>
            </div>
            <div class="stat-label">Failed</div>
          </div>
          <div>
            <div>
              <xsl:attribute name="class">stat <xsl:choose>
                <xsl:when test="at:summary/@errors &gt; 0">fail</xsl:when>
                <xsl:otherwise></xsl:otherwise>
              </xsl:choose></xsl:attribute>
              <xsl:value-of select="at:summary/@errors"/>
            </div>
            <div class="stat-label">Errors</div>
          </div>
          <div>
            <div class="stat"><xsl:value-of select="at:summary/@total"/></div>
            <div class="stat-label">Total</div>
          </div>
        </div>

        <div class="section-title">Test Groups</div>
        <table class="data">
          <tr>
            <th>Group</th><th>Document No.</th><th>Tests</th>
            <th>Passed</th><th>Failed</th><th>Errors</th>
          </tr>
          <xsl:for-each select="at:groups/at:group">
            <tr>
              <td>
                <a>
                  <xsl:attribute name="href"><xsl:value-of select="@report"/></xsl:attribute>
                  <xsl:value-of select="@name"/>
                </a>
              </td>
              <td><xsl:value-of select="@document-number"/></td>
              <td><xsl:value-of select="@tests"/></td>
              <td class="pass"><xsl:value-of select="@passed"/></td>
              <td>
                <xsl:attribute name="class">
                  <xsl:choose>
                    <xsl:when test="@failed &gt; 0">fail</xsl:when>
                    <xsl:otherwise>pass</xsl:otherwise>
                  </xsl:choose>
                </xsl:attribute>
                <xsl:value-of select="@failed"/>
              </td>
              <td>
                <xsl:attribute name="class">
                  <xsl:choose>
                    <xsl:when test="@errors &gt; 0">fail</xsl:when>
                    <xsl:otherwise></xsl:otherwise>
                  </xsl:choose>
                </xsl:attribute>
                <xsl:value-of select="@errors"/>
              </td>
            </tr>
          </xsl:for-each>
        </table>

        <div class="section-title">All Tests</div>
        <table class="data">
          <tr>
            <th>Test</th><th>Group</th><th>Status</th><th>Properties</th>
          </tr>
          <xsl:for-each select="at:tests/at:test">
            <tr>
              <td><xsl:value-of select="@name"/></td>
              <td><xsl:value-of select="@group"/></td>
              <td>
                <xsl:attribute name="class">
                  <xsl:choose>
                    <xsl:when test="@status = 'pass'">pass</xsl:when>
                    <xsl:when test="@status = 'fail'">fail</xsl:when>
                    <xsl:otherwise>fail</xsl:otherwise>
                  </xsl:choose>
                </xsl:attribute>
                <strong>
                  <xsl:value-of select="translate(@status,
                    'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')"/>
                </strong>
              </td>
              <td><xsl:value-of select="@properties"/></td>
            </tr>
          </xsl:for-each>
        </table>
      </div>
    </body>
    </html>
  </xsl:template>

</xsl:stylesheet>
