#include <catch2/catch_test_macros.hpp>
#include "interface/canopen/c_eds_parser.hpp"

using namespace interface;
using namespace interface::canopen;

TEST_CASE("EDS parser: basic variable object", "[canopen][eds]") {
    auto content = R"(
[FileInfo]
FileName=test.eds
FileVersion=1
CreatedBy=Test

[DeviceInfo]
VendorName=TestVendor
ProductName=TestProduct
VendorNumber=0x0001
ProductNumber=0x0002

[MandatoryObjects]
SupportedObjects=1
1=0x1000

[1000]
ParameterName=Device Type
ObjectType=0x07
DataType=0x0007
AccessType=ro
DefaultValue=0x000F0191
)";

    c_eds_parser parser;
    auto result = parser.parse_string(content);
    REQUIRE(result.has_value());

    auto& od = *result;
    REQUIRE(od.size() == 1);

    auto entry = od.find(0x1000);
    REQUIRE(entry.has_value());
    REQUIRE(entry->get().name == "Device Type");
    REQUIRE(entry->get().object_type == e_od_object_type::variable);
    REQUIRE(entry->get().sub_entries.size() == 1);

    auto& sub = entry->get().sub_entries[0];
    REQUIRE(sub.sub_index == 0);
    REQUIRE(sub.data_type == e_od_data_type::unsigned32);
    REQUIRE(sub.access == e_od_access::read_only);
}

TEST_CASE("EDS parser: record object with sub-entries", "[canopen][eds]") {
    auto content = R"(
[MandatoryObjects]
SupportedObjects=1
1=0x1018

[1018]
ParameterName=Identity Object
ObjectType=0x09
SubNumber=3

[1018sub0]
ParameterName=Number of Entries
DataType=0x0005
AccessType=ro
DefaultValue=2

[1018sub1]
ParameterName=Vendor ID
DataType=0x0007
AccessType=ro
DefaultValue=0x12345678

[1018sub2]
ParameterName=Product Code
DataType=0x0007
AccessType=ro
DefaultValue=0x0001
)";

    c_eds_parser parser;
    auto result = parser.parse_string(content);
    REQUIRE(result.has_value());

    auto entry = result->find(0x1018);
    REQUIRE(entry.has_value());
    REQUIRE(entry->get().name == "Identity Object");
    REQUIRE(entry->get().object_type == e_od_object_type::record);
    REQUIRE(entry->get().sub_entries.size() == 3);

    REQUIRE(entry->get().sub_entries[0].name == "Number of Entries");
    REQUIRE(entry->get().sub_entries[0].data_type == e_od_data_type::unsigned8);
    REQUIRE(entry->get().sub_entries[1].name == "Vendor ID");
    REQUIRE(entry->get().sub_entries[1].data_type == e_od_data_type::unsigned32);
    REQUIRE(entry->get().sub_entries[2].name == "Product Code");
}

TEST_CASE("EDS parser: multiple object categories", "[canopen][eds]") {
    auto content = R"(
[MandatoryObjects]
SupportedObjects=1
1=0x1000

[1000]
ParameterName=Device Type
ObjectType=0x07
DataType=0x0007
AccessType=ro

[OptionalObjects]
SupportedObjects=1
1=0x1001

[1001]
ParameterName=Error Register
ObjectType=0x07
DataType=0x0005
AccessType=ro

[ManufacturerSpecificObjects]
SupportedObjects=1
1=0x2000

[2000]
ParameterName=Custom Object
ObjectType=0x07
DataType=0x0007
AccessType=rw
)";

    c_eds_parser parser;
    auto result = parser.parse_string(content);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);
    REQUIRE(result->find(0x1000).has_value());
    REQUIRE(result->find(0x1001).has_value());
    REQUIRE(result->find(0x2000).has_value());
}

TEST_CASE("EDS parser: file info and device info", "[canopen][eds]") {
    auto content = R"(
[FileInfo]
FileName=device.eds
FileVersion=2
FileRevision=1
Description=Test device
CreationDate=01-01-2024
CreatedBy=TestTool

[DeviceInfo]
VendorName=ACME
ProductName=Widget
VendorNumber=42
ProductNumber=100
RevisionNumber=3

[MandatoryObjects]
SupportedObjects=0
)";

    c_eds_parser parser;
    auto result = parser.parse_string(content);
    REQUIRE(result.has_value());

    auto& fi = parser.file_info();
    REQUIRE(fi.file_name == "device.eds");
    REQUIRE(fi.file_version == "2");
    REQUIRE(fi.description == "Test device");
    REQUIRE(fi.created_by == "TestTool");

    auto& di = parser.device_info();
    REQUIRE(di.vendor_name == "ACME");
    REQUIRE(di.product_name == "Widget");
    REQUIRE(di.vendor_number == 42);
    REQUIRE(di.product_number == 100);
    REQUIRE(di.revision_number == 3);
}

TEST_CASE("EDS parser: access types", "[canopen][eds]") {
    auto content = R"(
[MandatoryObjects]
SupportedObjects=4
1=0x2000
2=0x2001
3=0x2002
4=0x2003

[2000]
ParameterName=ReadOnly
ObjectType=0x07
DataType=0x0005
AccessType=ro

[2001]
ParameterName=WriteOnly
ObjectType=0x07
DataType=0x0005
AccessType=wo

[2002]
ParameterName=ReadWrite
ObjectType=0x07
DataType=0x0005
AccessType=rw

[2003]
ParameterName=Constant
ObjectType=0x07
DataType=0x0005
AccessType=const
)";

    c_eds_parser parser;
    auto result = parser.parse_string(content);
    REQUIRE(result.has_value());

    REQUIRE(result->find(0x2000)->get().sub_entries[0].access == e_od_access::read_only);
    REQUIRE(result->find(0x2001)->get().sub_entries[0].access == e_od_access::write_only);
    REQUIRE(result->find(0x2002)->get().sub_entries[0].access == e_od_access::read_write);
    REQUIRE(result->find(0x2003)->get().sub_entries[0].access == e_od_access::constant);
}

TEST_CASE("EDS parser: empty content returns empty OD", "[canopen][eds]") {
    c_eds_parser parser;
    auto result = parser.parse_string("");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 0);
}

TEST_CASE("EDS parser: non-existent file", "[canopen][eds]") {
    c_eds_parser parser;
    auto result = parser.parse("/tmp/nonexistent_eds_12345.eds");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().category == e_error_category::io);
}
