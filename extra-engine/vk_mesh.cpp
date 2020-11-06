﻿#include <vk_mesh.h>
#include <tiny_obj_loader.h>
#include <iostream>
#include <asset_loader.h>
#include <mesh_asset.h>

VertexInputDescription Vertex::get_vertex_description()
{
	VertexInputDescription description;

	//we will have just 1 vertex buffer binding, with a per-vertex rate
	VkVertexInputBindingDescription mainBinding = {};
	mainBinding.binding = 0;
	mainBinding.stride = sizeof(Vertex);
	mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	description.bindings.push_back(mainBinding);

	//Position will be stored at Location 0
	VkVertexInputAttributeDescription positionAttribute = {};
	positionAttribute.binding = 0;
	positionAttribute.location = 0;
	positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	positionAttribute.offset = offsetof(Vertex, position);

	//Normal will be stored at Location 1
	VkVertexInputAttributeDescription normalAttribute = {};
	normalAttribute.binding = 0;
	normalAttribute.location = 1;
	normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	normalAttribute.offset = offsetof(Vertex, normal);

	//Position will be stored at Location 2
	VkVertexInputAttributeDescription colorAttribute = {};
	colorAttribute.binding = 0;
	colorAttribute.location = 2;
	colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	colorAttribute.offset = offsetof(Vertex, color);

	//UV will be stored at Location 2
	VkVertexInputAttributeDescription uvAttribute = {};
	uvAttribute.binding = 0;
	uvAttribute.location = 3;
	uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
	uvAttribute.offset = offsetof(Vertex, uv);


	description.attributes.push_back(positionAttribute);
	description.attributes.push_back(normalAttribute);
	description.attributes.push_back(colorAttribute);
	description.attributes.push_back(uvAttribute);
	return description;
}

bool Mesh::load_from_obj(const char* filename)
{
	//attrib will contain the vertex arrays of the file
	tinyobj::attrib_t attrib;
	//shapes contains the info for each separate object in the file
	std::vector<tinyobj::shape_t> shapes;
	//materials contains the information about the material of each shape, but we wont use it.
	std::vector<tinyobj::material_t> materials;

	//error and warning output from the load function
	std::string warn;
	std::string err;

	//load the OBJ file
	tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename,
		nullptr);
	//make sure to output the warnings to the console, in case there are issues with the file
	if (!warn.empty()) {
		std::cout << "WARN: " << warn << std::endl;
	}
	//if we have any error, print it to the console, and break the mesh loading. 
	//This happens if the file cant be found or is malformed
	if (!err.empty()) {
		std::cerr << err << std::endl;
		return false;
	}

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) {
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {

			//hardcode loading to triangles
			int fv = 3;

			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

				//vertex position
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
				//vertex normal
				tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
				tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
				tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

				//vertex uv
				tinyobj::real_t ux = attrib.texcoords[2 * idx.texcoord_index + 0];
				tinyobj::real_t uy = attrib.texcoords[2 * idx.texcoord_index + 1];

				//copy it into our vertex
				Vertex new_vert;
				new_vert.position.x = vx;
				new_vert.position.y = vy;
				new_vert.position.z = vz;

				new_vert.normal.x = nx;
				new_vert.normal.y = ny;
				new_vert.normal.z = nz;


				new_vert.uv.x = ux;
				new_vert.uv.y = 1-uy;

				//we are setting the vertex color as the vertex normal. This is just for display purposes
				new_vert.color = new_vert.normal;


				_vertices.push_back(new_vert);
			}
			index_offset += fv;
		}
	}


	for (int i = 0; i < _vertices.size(); i++) {
		_indices.push_back(i);
	}
	bounds.valid = false;
	return true;
}

bool Mesh::load_from_meshasset(const char* filename)
{
	assets::AssetFile file;
	bool loaded = assets::load_binaryfile(filename, file);

	if (!loaded) {
		std::cout << "Error when loading mesh " << filename << std::endl;;
		return false;
	}

	assets::MeshInfo meshinfo= assets::read_mesh_info(&file);

	

	std::vector<char> vertexBuffer;
	std::vector<char> indexBuffer;

	vertexBuffer.resize(meshinfo.vertexBuferSize);
	indexBuffer.resize(meshinfo.indexBuferSize);

	assets::unpack_mesh(&meshinfo, file.binaryBlob.data(), file.binaryBlob.size(), vertexBuffer.data(), indexBuffer.data());

	bounds.extents.x = meshinfo.bounds.extents[0];
	bounds.extents.y = meshinfo.bounds.extents[1];
	bounds.extents.z = meshinfo.bounds.extents[2];

	bounds.origin.x = meshinfo.bounds.origin[0];
	bounds.origin.y = meshinfo.bounds.origin[1];
	bounds.origin.z = meshinfo.bounds.origin[2];

	bounds.radius = meshinfo.bounds.radius;
	bounds.valid = true;

	_vertices.clear();


	_indices.clear();

	_indices.resize(indexBuffer.size() / sizeof(uint32_t));
	for (int i = 0; i < _indices.size(); i++) {
		uint32_t* unpacked_indices = (uint32_t*)indexBuffer.data();
		_indices[i] = unpacked_indices[i];
	}

	if (meshinfo.vertexFormat == assets::VertexFormat::PNCV_F32)
	{	
		assets::Vertex_f32_PNCV* unpackedVertices = (assets::Vertex_f32_PNCV*)vertexBuffer.data();

		_vertices.resize(vertexBuffer.size() / sizeof(assets::Vertex_f32_PNCV));

		for (int i = 0; i < _vertices.size(); i++) {

			_vertices[i].position.x = unpackedVertices[i].position[0];
			_vertices[i].position.y = unpackedVertices[i].position[1];
			_vertices[i].position.z = unpackedVertices[i].position[2];

			_vertices[i].normal.x = unpackedVertices[i].normal[0];
			_vertices[i].normal.y = unpackedVertices[i].normal[1];
			_vertices[i].normal.z = unpackedVertices[i].normal[2];

			_vertices[i].color.x = unpackedVertices[i].color[0];
			_vertices[i].color.y = unpackedVertices[i].color[1];
			_vertices[i].color.z = unpackedVertices[i].color[2];

			_vertices[i].uv.x = unpackedVertices[i].uv[0];
			_vertices[i].uv.y = unpackedVertices[i].uv[1];
		}
	}
	else if (meshinfo.vertexFormat == assets::VertexFormat::P32N8C8V16)
	{
		assets::Vertex_P32N8C8V16* unpackedVertices = (assets::Vertex_P32N8C8V16*)vertexBuffer.data();

		_vertices.resize(vertexBuffer.size() / sizeof(assets::Vertex_P32N8C8V16));

		for (int i = 0; i < _vertices.size(); i++) {

			_vertices[i].position.x = unpackedVertices[i].position[0];
			_vertices[i].position.y = unpackedVertices[i].position[1];
			_vertices[i].position.z = unpackedVertices[i].position[2];

			_vertices[i].normal.x = ((unpackedVertices[i].normal[0] / 255.f) * 2.0) - 1.f;
			_vertices[i].normal.y = ((unpackedVertices[i].normal[1] / 255.f) * 2.0) - 1.f;
			_vertices[i].normal.z = ((unpackedVertices[i].normal[2] / 255.f) * 2.0) - 1.f;

			_vertices[i].color.x = unpackedVertices[i].color[0] / 255.f;
			_vertices[i].color.y = unpackedVertices[i].color[1] / 255.f;
			_vertices[i].color.z = unpackedVertices[i].color[2] / 255.f;

			_vertices[i].uv.x = unpackedVertices[i].uv[0];
			_vertices[i].uv.y = unpackedVertices[i].uv[1];
		}
	}
	return true;
}
