/*
  Q Light Controller Plus - Unit test
*/

#ifndef QUERY_TOOLS_TEST_H
#define QUERY_TOOLS_TEST_H

#include <QObject>

class QueryTools_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void queryPalettes_invalidTypeFilterReturnsError();
    void palettes_createQueryRoundTrip_data();
    void palettes_createQueryRoundTrip();
    void palettes_zoomValue_data();
    void deletePalettes_perSelectorRecords();
    void palettes_zoomValue();
    void palettes_createInvalidTypeReturnsError();
    void palettes_updateModifiedAndFloatReadback();
    void palettes_invalidValueKind_data();
    void palettes_invalidValueKind();
    void queryRgbAlgorithms_invalidTypeReturnsError();
    void queryRgbAlgorithms_matrixTypeSchema();
    void queryRgbAlgorithms_matrixType_data();
    void queryRgbAlgorithms_matrixType();
    void queryRgbAlgorithms_floatBoundsMetadata();
    void queryRgbAlgorithms_invalidMatrixTypeReturnsError();
    void queryWorkspaceSummary_returnsExpectedCounts();
    void queryWorkspaceSummary_populatedDoc_returnsExactCounts();
    void queryFixtures_legacyShapeAndFilters();
    void queryFixtures_cursorPagination();
    void queryFixtures_invalidPage_data();
    void queryFixtures_invalidPage();
    void updateFixture_atomicAndIdempotent();
    void updateFixture_invalidRequest_data();
    void updateFixture_invalidRequest();
    void patchFixtures_schemaDescribesExactMatch();
    void patchFixtures_invalidBounds_data();
    void patchFixtures_invalidBounds();
    void patchFixtures_quantityIsAtomicPerItem();
    void queryFixtureChannels_invalidFixtureID_data();
    void queryFixtureChannels_invalidFixtureID();
    void configureChannels_invalidReference_data();
    void configureChannels_invalidReference();
    void configureChannels_precedenceAndModified_data();
    void configureChannels_precedenceAndModified();
    void setChannelModifiers_marksModified();
    void channelConfig_survivesSaveReload_data();
    void channelConfig_survivesSaveReload();
    void transport_toolResultEncoding_data();
    void transport_toolResultEncoding();

private:
    class Doc *m_doc = nullptr;
};

#endif // QUERY_TOOLS_TEST_H
