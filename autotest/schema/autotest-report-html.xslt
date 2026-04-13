<?xml version="1.0" encoding="UTF-8"?>
<!--
  autotest-report-html.xslt
  Transforms <autotest-report> XML into HTML for browser viewing.
  Referenced via <?xml-stylesheet?> PI in the XML files.
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:at="urn:aviastorm:autotest-results:1.0">

  <xsl:output method="html" indent="yes" encoding="UTF-8"/>

  <xsl:template match="/at:autotest-report">
    <html>
    <head>
      <meta charset="UTF-8"/>
      <title><xsl:value-of select="at:metadata/at:title"/></title>
      <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: -apple-system, 'Segoe UI', Helvetica, Arial, sans-serif;
               font-size: 14px; color: #222; background: #f8f9fa; padding: 20px; }
        .container { max-width: 1200px; margin: 0 auto; }

        /* Banner */
        .banner { background: #1B3A5C; color: #fff; text-align: center;
                  padding: 12px 20px; font-size: 18px; font-weight: bold;
                  letter-spacing: 1px; margin-bottom: 16px; }
        .banner-table { width: 100%; background: #1B3A5C; color: #fff;
                        margin-bottom: 16px; border-collapse: collapse; }
        .banner-logo { padding: 8px 16px; width: 80px; vertical-align: middle; }
        .banner-logo-img { max-height: 48px; max-width: 64px; }
        .banner-company { padding: 8px 16px; font-size: 16px; font-weight: bold;
                          letter-spacing: 1px; }
        .banner-title { padding: 4px 16px 8px; font-size: 18px; font-weight: bold;
                        letter-spacing: 1px; }

        /* Document control */
        .doc-control { display: grid; grid-template-columns: auto 1fr auto;
                       border: 1px solid #1B3A5C; margin-bottom: 12px; }
        .doc-control .hdr { background: #1B3A5C; color: #fff; padding: 6px 12px;
                            font-weight: bold; font-size: 12px; }
        .doc-control .val { padding: 6px 12px; border: 1px solid #e0e0e0; }

        /* Metadata table */
        .meta-table { width: 100%; border-collapse: collapse; margin-bottom: 16px; }
        .meta-table td { padding: 4px 12px; border-bottom: 1px solid #e8e8e8; }
        .meta-table .label { background: #f0f0f0; font-weight: bold; width: 180px;
                             font-size: 12px; text-transform: uppercase; }

        /* Summary bar */
        .summary-bar { display: flex; gap: 16px; margin-bottom: 20px;
                       padding: 10px 16px; background: #fff; border: 1px solid #ddd;
                       border-radius: 4px; }
        .summary-bar .stat { font-size: 20px; font-weight: bold; }
        .summary-bar .stat-label { font-size: 11px; color: #666;
                                    text-transform: uppercase; }
        .pass { color: #1A7A1A; }
        .fail { color: #CC0000; }

        /* Test card */
        .test-card { background: #fff; border: 1px solid #ddd; border-radius: 4px;
                     margin-bottom: 16px; overflow: hidden; }
        .test-header { padding: 10px 16px; border-bottom: 1px solid #eee;
                       display: flex; align-items: center; gap: 12px;
                       cursor: pointer; }
        .test-header:hover { background: #f8f8f8; }
        .test-name { font-size: 15px; font-weight: bold; color: #1B3A5C; flex: 1; }
        .test-desc { font-style: italic; color: #555; font-size: 13px;
                     padding: 4px 16px 8px; }
        .badge { display: inline-block; padding: 2px 10px; border-radius: 3px;
                 font-size: 12px; font-weight: bold; color: #fff; }
        .badge-pass { background: #1A7A1A; }
        .badge-fail { background: #CC0000; }
        .badge-error { background: #CC0000; }
        .counts { font-size: 12px; color: #666; }

        .test-body { padding: 12px 16px; }

        /* Tables */
        table.data { width: 100%; border-collapse: collapse; margin-bottom: 12px;
                     font-size: 13px; }
        table.data th { background: #1B3A5C; color: #fff; padding: 5px 8px;
                        text-align: left; font-size: 12px; }
        table.data td { padding: 4px 8px; border-bottom: 1px solid #e8e8e8; }
        table.data tr:nth-child(even) { background: #f8f8f8; }
        table.data .prop-name { font-family: 'Consolas', 'Monaco', monospace;
                                font-size: 12px; word-break: break-all; }

        .section-title { font-size: 13px; font-weight: bold; color: #1B3A5C;
                         margin: 10px 0 4px; border-bottom: 1px solid #ddd;
                         padding-bottom: 2px; }

        /* Plot image */
        .plot-title { font-size: 13px; font-weight: bold; color: #1B3A5C;
                      margin: 8px 0 4px; }
        .plot-img { max-width: 100%; height: auto; border: 1px solid #ddd;
                    margin-bottom: 8px; }

        /* Collapsible */
        .collapsible { display: none; }
        .test-card.open .collapsible { display: block; }
        .toggle-icon { font-size: 12px; color: #999; transition: transform 0.2s; }
        .test-card.open .toggle-icon { transform: rotate(90deg); }
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
                <td class="banner-title">AUTOTEST REPORT</td>
              </tr>
            </table>
          </xsl:when>
          <xsl:otherwise>
            <div class="banner">AUTOTEST REPORT</div>
          </xsl:otherwise>
        </xsl:choose>

        <!-- Document control -->
        <div class="doc-control">
          <div class="hdr">DOCUMENT NO.</div>
          <div class="hdr">TITLE</div>
          <div class="hdr">DATE</div>
          <div class="val"><xsl:value-of select="at:metadata/at:document-number"/></div>
          <div class="val"><xsl:value-of select="at:metadata/at:title"/></div>
          <div class="val"><xsl:value-of select="at:metadata/at:date"/></div>
        </div>

        <!-- Metadata -->
        <table class="meta-table">
          <xsl:if test="at:metadata/at:aircraft != ''">
            <tr><td class="label">Aircraft</td><td><xsl:value-of select="at:metadata/at:aircraft"/></td></tr>
          </xsl:if>
          <xsl:if test="at:metadata/at:system != ''">
            <tr><td class="label">System</td><td><xsl:value-of select="at:metadata/at:system"/></td></tr>
          </xsl:if>
          <xsl:if test="at:metadata/at:ata-chapter != ''">
            <tr><td class="label">ATA Chapter</td><td><xsl:value-of select="at:metadata/at:ata-chapter"/></td></tr>
          </xsl:if>
          <xsl:if test="at:metadata/at:reference != ''">
            <tr><td class="label">Reference</td><td><xsl:value-of select="at:metadata/at:reference"/></td></tr>
          </xsl:if>
          <tr><td class="label">Revision</td><td><xsl:value-of select="at:metadata/at:revision"/></td></tr>
          <xsl:if test="at:metadata/at:prepared-by != ''">
            <tr><td class="label">Prepared By</td><td><xsl:value-of select="at:metadata/at:prepared-by"/></td></tr>
          </xsl:if>
          <tr><td class="label">Classification</td><td><xsl:value-of select="at:metadata/at:classification"/></td></tr>
        </table>

        <!-- Summary -->
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

        <!-- Test cards -->
        <xsl:for-each select="at:test">
          <div class="test-card" id="test-{position()}">
            <div class="test-header" onclick="this.parentElement.classList.toggle('open')">
              <span class="toggle-icon">&#9654;</span>
              <span class="test-name"><xsl:value-of select="@name"/></span>
              <span class="counts">
                <xsl:if test="at:result/@properties-total">
                  <xsl:value-of select="at:result/@properties-passed"/>/<xsl:value-of select="at:result/@properties-total"/> props
                </xsl:if>
                <xsl:if test="at:result/@checks-total">
                  <xsl:text>, </xsl:text>
                  <xsl:value-of select="at:result/@checks-passed"/>/<xsl:value-of select="at:result/@checks-total"/> checks
                </xsl:if>
              </span>
              <span>
                <xsl:attribute name="class">badge <xsl:choose>
                  <xsl:when test="at:result/@status = 'pass'">badge-pass</xsl:when>
                  <xsl:when test="at:result/@status = 'fail'">badge-fail</xsl:when>
                  <xsl:otherwise>badge-error</xsl:otherwise>
                </xsl:choose></xsl:attribute>
                <xsl:value-of select="translate(at:result/@status,
                  'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')"/>
              </span>
            </div>

            <xsl:if test="at:description != ''">
              <div class="test-desc"><xsl:value-of select="at:description"/></div>
            </xsl:if>

            <div class="test-body collapsible">
              <!-- Error message -->
              <xsl:if test="at:result/at:message">
                <div class="fail" style="margin-bottom:8px;">
                  <xsl:value-of select="at:result/at:message"/>
                </div>
              </xsl:if>

              <!-- Property results -->
              <xsl:if test="at:property-results/at:property">
                <div class="section-title">Property Results</div>
                <table class="data">
                  <tr>
                    <th>Property</th><th>Max Delta</th><th>Tol (abs)</th>
                    <th>Tol (rel)</th><th>Baseline</th><th>Result</th>
                  </tr>
                  <xsl:for-each select="at:property-results/at:property">
                    <tr>
                      <td class="prop-name"><xsl:value-of select="@name"/></td>
                      <td><xsl:value-of select="@max-delta"/></td>
                      <td><xsl:choose>
                        <xsl:when test="@tol-abs"><xsl:value-of select="@tol-abs"/></xsl:when>
                        <xsl:otherwise>&#8212;</xsl:otherwise>
                      </xsl:choose></td>
                      <td><xsl:choose>
                        <xsl:when test="@tol-rel"><xsl:value-of select="@tol-rel"/></xsl:when>
                        <xsl:otherwise>&#8212;</xsl:otherwise>
                      </xsl:choose></td>
                      <td><xsl:choose>
                        <xsl:when test="@baseline-at-max"><xsl:value-of select="@baseline-at-max"/></xsl:when>
                        <xsl:otherwise>&#8212;</xsl:otherwise>
                      </xsl:choose></td>
                      <td>
                        <xsl:attribute name="class">
                          <xsl:choose>
                            <xsl:when test="@passed = 'true'">pass</xsl:when>
                            <xsl:otherwise>fail</xsl:otherwise>
                          </xsl:choose>
                        </xsl:attribute>
                        <strong>
                          <xsl:choose>
                            <xsl:when test="@passed = 'true'">PASS</xsl:when>
                            <xsl:otherwise>FAIL</xsl:otherwise>
                          </xsl:choose>
                        </strong>
                      </td>
                    </tr>
                  </xsl:for-each>
                </table>
              </xsl:if>

              <!-- Event Checks -->
              <xsl:if test="at:check-results/at:check">
                <div class="section-title">Event Checks</div>
                <table class="data">
                  <tr>
                    <th>Property</th><th>Time</th><th>Expected</th>
                    <th>Actual</th><th>Tolerance</th><th>Result</th><th>Message</th>
                  </tr>
                  <xsl:for-each select="at:check-results/at:check">
                    <tr>
                      <td class="prop-name"><xsl:value-of select="@property"/></td>
                      <td><xsl:choose>
                        <xsl:when test="@time"><xsl:value-of select="@time"/></xsl:when>
                        <xsl:otherwise>&#8212;</xsl:otherwise>
                      </xsl:choose></td>
                      <td><xsl:value-of select="@expected"/></td>
                      <td><xsl:value-of select="@actual"/></td>
                      <td><xsl:value-of select="@tolerance"/></td>
                      <td>
                        <xsl:attribute name="class">
                          <xsl:choose>
                            <xsl:when test="@passed = 'true'">pass</xsl:when>
                            <xsl:otherwise>fail</xsl:otherwise>
                          </xsl:choose>
                        </xsl:attribute>
                        <strong>
                          <xsl:choose>
                            <xsl:when test="@passed = 'true'">PASS</xsl:when>
                            <xsl:otherwise>FAIL</xsl:otherwise>
                          </xsl:choose>
                        </strong>
                      </td>
                      <td><xsl:value-of select="@message"/></td>
                    </tr>
                  </xsl:for-each>
                </table>
              </xsl:if>

              <!-- Initial conditions -->
              <xsl:if test="at:initial-conditions/at:condition">
                <div class="section-title">Initial Conditions</div>
                <table class="data">
                  <tr><th>Section</th><th>Frame</th><th>Values</th><th>Description</th></tr>
                  <xsl:for-each select="at:initial-conditions/at:condition">
                    <tr>
                      <td><xsl:value-of select="@section"/></td>
                      <td><xsl:value-of select="@frame"/></td>
                      <td><xsl:value-of select="@values"/></td>
                      <td><xsl:value-of select="@description"/></td>
                    </tr>
                  </xsl:for-each>
                </table>
              </xsl:if>

              <!-- Events -->
              <xsl:if test="at:events/at:event">
                <div class="section-title">Events</div>
                <xsl:for-each select="at:events/at:event">
                  <div style="margin: 6px 0 2px; font-weight:bold; color:#2D5F8A; font-size:13px;">
                    <xsl:value-of select="@name"/>
                    <span style="color:#666; font-weight:normal;">
                      <xsl:text> (t=</xsl:text><xsl:value-of select="@time"/><xsl:text>s)</xsl:text>
                    </span>
                  </div>
                  <xsl:if test="at:property">
                    <table class="data" style="width:80%;">
                      <tr><th>Property</th><th>Value</th></tr>
                      <xsl:for-each select="at:property">
                        <tr>
                          <td class="prop-name"><xsl:value-of select="@name"/></td>
                          <td><xsl:value-of select="@value"/></td>
                        </tr>
                      </xsl:for-each>
                    </table>
                  </xsl:if>
                </xsl:for-each>
              </xsl:if>

              <!-- Plot -->
              <xsl:for-each select="at:plots/at:plot">
                <div class="plot-title">
                  <xsl:value-of select="../../@name"/>
                  <xsl:text> &#8212; Overlay Plot</xsl:text>
                </div>
                <img class="plot-img">
                  <xsl:attribute name="src"><xsl:value-of select="@file"/></xsl:attribute>
                  <xsl:attribute name="alt"><xsl:value-of select="../../@name"/> overlay plot</xsl:attribute>
                </img>
              </xsl:for-each>
            </div>
          </div>
        </xsl:for-each>
      </div>

      <!-- Auto-open failed tests -->
      <script>
        document.querySelectorAll('.badge-fail, .badge-error').forEach(function(el) {
          el.closest('.test-card').classList.add('open');
        });
      </script>
    </body>
    </html>
  </xsl:template>

</xsl:stylesheet>
