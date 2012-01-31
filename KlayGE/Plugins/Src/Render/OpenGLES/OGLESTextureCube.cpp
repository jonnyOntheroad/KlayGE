// OGLESTextureCube.cpp
// KlayGE OpenGL ES 2 Cube������ ʵ���ļ�
// Ver 3.10.0
// ��Ȩ����(C) ������, 2010
// Homepage: http://www.klayge.org
//
// 3.10.0
// ���ν��� (2010.1.22)
//
// �޸ļ�¼
/////////////////////////////////////////////////////////////////////////////////

#include <KlayGE/KlayGE.hpp>
#include <KlayGE/Util.hpp>
#include <KlayGE/ThrowErr.hpp>
#include <KlayGE/Context.hpp>
#include <KlayGE/Math.hpp>
#include <KlayGE/RenderEngine.hpp>
#include <KlayGE/RenderFactory.hpp>
#include <KlayGE/Texture.hpp>

#include <cstring>

#include <glloader/glloader.h>
#ifndef KLAYGE_PLATFORM_ANDROID
#include <GL/glu.h>
#endif

#include <KlayGE/OpenGLES/OGLESRenderEngine.hpp>
#include <KlayGE/OpenGLES/OGLESMapping.hpp>
#include <KlayGE/OpenGLES/OGLESTexture.hpp>

#ifdef KLAYGE_COMPILER_MSVC
#pragma comment(lib, "glu32.lib")
#endif

namespace KlayGE
{
	OGLESTextureCube::OGLESTextureCube(uint32_t size, uint32_t numMipMaps, uint32_t array_size, ElementFormat format,
								uint32_t sample_count, uint32_t sample_quality, uint32_t access_hint, ElementInitData const * init_data)
					: OGLESTexture(TT_Cube, array_size, sample_count, sample_quality, access_hint)
	{
		if (IsSRGB(format))
		{
			format = this->SRGBToRGB(format);
		}

		format_ = format;

		if (0 == numMipMaps)
		{
			num_mip_maps_ = 1;
			uint32_t s = size;
			while (s > 1)
			{
				++ num_mip_maps_;

				s = std::max<uint32_t>(1U, s / 2);
			}
		}
		else
		{
			num_mip_maps_ = numMipMaps;
		}
		array_size_ = 1;

		uint32_t texel_size = NumFormatBytes(format_);

		GLint glinternalFormat;
		GLenum glformat;
		GLenum gltype;
		OGLESMapping::MappingFormat(glinternalFormat, glformat, gltype, format_);

		glGenTextures(1, &texture_);
		glBindTexture(target_type_, texture_);
		glTexParameteri(target_type_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(target_type_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		if (glloader_GLES_APPLE_texture_max_level())
		{
			glTexParameteri(target_type_, GL_TEXTURE_MAX_LEVEL_APPLE, num_mip_maps_ - 1);
		}

		tex_data_.resize(6 * num_mip_maps_);
		widthes_.resize(6 * num_mip_maps_);
		for (int face = 0; face < 6; ++ face)
		{
			uint32_t s = size;
			for (uint32_t level = 0; level < num_mip_maps_; ++ level)
			{
				widthes_[face * num_mip_maps_ + level] = s;

				if (IsCompressedFormat(format_))
				{
					int block_size;
					if ((EF_BC1 == format_) || (EF_SIGNED_BC1 == format_) || (EF_BC1_SRGB == format_)
						|| (EF_BC4 == format_) || (EF_SIGNED_BC4 == format_) || (EF_BC4_SRGB == format_))
					{
						block_size = 8;
					}
					else
					{
						block_size = 16;
					}

					GLsizei const image_size = ((s + 3) / 4) * ((s + 3) / 4) * block_size;

					if (NULL == init_data)
					{
						tex_data_[face * num_mip_maps_ + level].resize(image_size, 0);
					}
					else
					{
						tex_data_[face * num_mip_maps_ + level].resize(image_size);
						memcpy(&tex_data_[face * num_mip_maps_ + level][0], init_data[face * num_mip_maps_ + level].data, image_size);
					}
					glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, glinternalFormat,
						s, s, 0, image_size, &tex_data_[face * num_mip_maps_ + level][0]);
				}
				else
				{
					GLsizei const image_size = s * s * texel_size;

					if (NULL == init_data)
					{
						tex_data_[face * num_mip_maps_ + level].resize(image_size, 0);
					}
					else
					{
						tex_data_[face * num_mip_maps_ + level].resize(image_size);
						memcpy(&tex_data_[face * num_mip_maps_ + level][0], init_data[face * num_mip_maps_ + level].data, image_size);
					}
					glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, glinternalFormat,
						s, s, 0, glformat, gltype, &tex_data_[face * num_mip_maps_ + level][0]);
				}

				s = std::max(1U, s / 2);
			}
		}
	}

	uint32_t OGLESTextureCube::Width(uint32_t level) const
	{
		BOOST_ASSERT(level < num_mip_maps_);

		return widthes_[level];
	}

	uint32_t OGLESTextureCube::Height(uint32_t level) const
	{
		return this->Width(level);
	}

	void OGLESTextureCube::CopyToTexture(Texture& target)
	{
		BOOST_ASSERT(type_ == target.Type());

		if ((format_ == target.Format()) && (widthes_[0] == target.Width(0)))
		{
			uint32_t texel_size = NumFormatBytes(format_);

			GLint gl_internalFormat;
			GLenum gl_format;
			GLenum gl_type;
			OGLESMapping::MappingFormat(gl_internalFormat, gl_format, gl_type, format_);

			OGLESTextureCube& gles_target = *checked_cast<OGLESTextureCube*>(&target);
			for (int face = 0; face < 6; ++ face)
			{
				for (uint32_t level = 0; level < num_mip_maps_; ++ level)
				{
					glBindTexture(target_type_, gles_target.GLTexture());

					if (IsCompressedFormat(format_))
					{
						int block_size;
						if ((EF_BC1 == format_) || (EF_SIGNED_BC1 == format_) || (EF_BC1_SRGB == format_)
							|| (EF_BC4 == format_) || (EF_SIGNED_BC4 == format_) || (EF_BC4_SRGB == format_))
						{
							block_size = 8;
						}
						else
						{
							block_size = 16;
						}

						GLsizei const image_size = ((this->Width(level) + 3) / 4) * ((this->Width(level) + 3) / 4) * block_size;

						memcpy(&gles_target.tex_data_[face * num_mip_maps_ + level][0], &tex_data_[face * num_mip_maps_ + level][0], image_size);
						glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, 0, 0,
							this->Width(level), this->Width(level), gl_format, image_size, &tex_data_[face * num_mip_maps_ + level][0]);
					}
					else
					{
						GLsizei const image_size = target.Width(level) * target.Width(level) * texel_size;

						memcpy(&gles_target.tex_data_[face * num_mip_maps_ + level][0], &tex_data_[face * num_mip_maps_ + level][0], image_size);
						glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, 0, 0, this->Width(level), this->Width(level),
								gl_format, gl_type, &tex_data_[face * num_mip_maps_ + level][0]);
					}
				}
			}
		}
		else
		{
			for (uint32_t array_index = 0; array_index < array_size_; ++ array_index)
			{
				for (int face = 0; face < 6; ++ face)
				{
					for (uint32_t level = 0; level < num_mip_maps_; ++ level)
					{
						this->CopyToSubTextureCube(target,
							array_index, static_cast<CubeFaces>(face), level, 0, 0, target.Width(level), target.Height(level),
							array_index, static_cast<CubeFaces>(face), level, 0, 0, this->Width(level), this->Height(level));
					}
				}
			}
		}
	}

	void OGLESTextureCube::CopyToSubTextureCube(Texture& target,
			uint32_t dst_array_index, CubeFaces dst_face, uint32_t dst_level, uint32_t dst_x_offset, uint32_t dst_y_offset, uint32_t dst_width, uint32_t dst_height,
			uint32_t src_array_index, CubeFaces src_face, uint32_t src_level, uint32_t src_x_offset, uint32_t src_y_offset, uint32_t src_width, uint32_t src_height)
	{
		BOOST_ASSERT(type_ == target.Type());

		BOOST_ASSERT(format_ == target.Format());

		GLint gl_internalFormat;
		GLenum gl_format;
		GLenum gl_type;
		OGLESMapping::MappingFormat(gl_internalFormat, gl_format, gl_type, format_);

		GLint gl_target_internal_format;
		GLenum gl_target_format;
		GLenum gl_target_type;
		OGLESMapping::MappingFormat(gl_target_internal_format, gl_target_format, gl_target_type, target.Format());

		if (IsCompressedFormat(format_))
		{
			BOOST_ASSERT((src_width == dst_width) && (src_height == dst_height));
			BOOST_ASSERT((0 == (src_x_offset & 0x3)) && (0 == (src_y_offset & 0x3)));
			BOOST_ASSERT((0 == (dst_x_offset & 0x3)) && (0 == (dst_y_offset & 0x3)));
			BOOST_ASSERT((0 == (src_width & 0x3)) && (0 == (src_height & 0x3)));
			BOOST_ASSERT((0 == (dst_width & 0x3)) && (0 == (dst_height & 0x3)));

			Texture::Mapper mapper_src(*this, src_array_index, src_face, src_level, TMA_Read_Only, 0, 0, this->Width(src_level), this->Height(src_level));
			Texture::Mapper mapper_dst(target, dst_array_index, dst_face, dst_level, TMA_Write_Only, 0, 0, target.Width(dst_level), target.Height(dst_level));

			int block_size;
			if ((EF_BC1 == format_) || (EF_SIGNED_BC1 == format_) || (EF_BC1_SRGB == format_)
				|| (EF_BC4 == format_) || (EF_SIGNED_BC4 == format_) || (EF_BC4_SRGB == format_))
			{
				block_size = 8;
			}
			else
			{
				block_size = 16;
			}

			uint8_t const * s = mapper_src.Pointer<uint8_t>() + (src_y_offset / 4) * mapper_src.RowPitch() + (src_x_offset / 4 * block_size);
			uint8_t* d = mapper_dst.Pointer<uint8_t>() + (dst_y_offset / 4) * mapper_dst.RowPitch() + (dst_x_offset / 4 * block_size);
			for (uint32_t y = 0; y < src_height; y += 4)
			{
				memcpy(d, s, src_width / 4 * block_size);

				s += mapper_src.RowPitch();
				d += mapper_dst.RowPitch();
			}
		}
		else
		{
			size_t const src_format_size = NumFormatBytes(format_);
			size_t const dst_format_size = NumFormatBytes(target.Format());

			if ((src_width != dst_width) || (src_height != dst_height))
			{
#ifndef KLAYGE_PLATFORM_ANDROID
				std::vector<uint8_t> data_in(src_width * src_height * src_format_size);
				std::vector<uint8_t> data_out(dst_width * dst_height * dst_format_size);

				{
					Texture::Mapper mapper(*this, src_array_index, src_face, src_level, TMA_Read_Only, src_x_offset, src_y_offset, src_width, src_height);
					uint8_t const * s = mapper.Pointer<uint8_t>();
					uint8_t* d = &data_in[0];
					for (uint32_t y = 0; y < src_height; ++ y)
					{
						memcpy(d, s, src_width * src_format_size);

						s += mapper.RowPitch();
						d += src_width * src_format_size;
					}
				}

				gluScaleImage(gl_format, src_width, src_height, gl_type, &data_in[0],
					dst_width, dst_height, gl_target_type, &data_out[0]);

				{
					Texture::Mapper mapper(target, dst_array_index, dst_face, dst_level, TMA_Write_Only, dst_x_offset, dst_y_offset, dst_width, dst_height);
					uint8_t const * s = &data_out[0];
					uint8_t* d = mapper.Pointer<uint8_t>();
					for (uint32_t y = 0; y < src_height; ++ y)
					{
						memcpy(d, s, dst_width * dst_format_size);

						s += src_width * src_format_size;
						d += mapper.RowPitch();
					}
				}
#else
				BOOST_ASSERT(false);
#endif
			}
			else
			{
				Texture::Mapper mapper_src(*this, src_array_index, src_face, src_level, TMA_Read_Only, src_x_offset, src_y_offset, src_width, src_height);
				Texture::Mapper mapper_dst(target, dst_array_index, dst_face, dst_level, TMA_Write_Only, dst_x_offset, dst_y_offset, dst_width, dst_height);
				uint8_t const * s = mapper_src.Pointer<uint8_t>();
				uint8_t* d = mapper_dst.Pointer<uint8_t>();
				for (uint32_t y = 0; y < src_height; ++ y)
				{
					memcpy(d, s, src_width * src_format_size);

					s += mapper_src.RowPitch();
					d += mapper_dst.RowPitch();
				}
			}
		}
	}

	void OGLESTextureCube::MapCube(uint32_t array_index, CubeFaces face, uint32_t level, TextureMapAccess tma,
					uint32_t x_offset, uint32_t y_offset, uint32_t /*width*/, uint32_t /*height*/,
					void*& data, uint32_t& row_pitch)
	{
		BOOST_ASSERT(0 == array_index);
		UNREF_PARAM(array_index);

		last_tma_ = tma;

		uint32_t const texel_size = NumFormatBytes(format_);
		int block_size;
		if (IsCompressedFormat(format_))
		{
			if ((EF_BC1 == format_) || (EF_SIGNED_BC1 == format_) || (EF_BC1_SRGB == format_)
				|| (EF_BC4 == format_) || (EF_SIGNED_BC4 == format_) || (EF_BC4_SRGB == format_))
			{
				block_size = 8;
			}
			else
			{
				block_size = 16;
			}
		}
		else
		{
			block_size = 0;
		}

		row_pitch = widthes_[level] * texel_size;

		uint8_t* p = &tex_data_[face * num_mip_maps_ + level][0];
		if (IsCompressedFormat(format_))
		{
			data = p + (y_offset / 4) * row_pitch + (x_offset / 4 * block_size);
		}
		else
		{
			data = p + (y_offset * widthes_[level] + x_offset) * texel_size;
		}
	}

	void OGLESTextureCube::UnmapCube(uint32_t array_index, CubeFaces face, uint32_t level)
	{
		BOOST_ASSERT(0 == array_index);
		UNREF_PARAM(array_index);

		switch (last_tma_)
		{
		case TMA_Read_Only:
			break;

		case TMA_Write_Only:
		case TMA_Read_Write:
			{
				GLint gl_internalFormat;
				GLenum gl_format;
				GLenum gl_type;
				OGLESMapping::MappingFormat(gl_internalFormat, gl_format, gl_type, format_);

				glBindTexture(target_type_, texture_);

				if (IsCompressedFormat(format_))
				{
					int block_size;
					if ((EF_BC1 == format_) || (EF_SIGNED_BC1 == format_) || (EF_BC1_SRGB == format_)
						|| (EF_BC4 == format_) || (EF_SIGNED_BC4 == format_) || (EF_BC4_SRGB == format_))
					{
						block_size = 8;
					}
					else
					{
						block_size = 16;
					}

					GLsizei const image_size = ((this->Width(level) + 3) / 4) * ((this->Height(level) + 3) / 4) * block_size;

					glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level,
						0, 0, widthes_[level], widthes_[level], gl_format, image_size, &tex_data_[face * num_mip_maps_ + level][0]);
				}
				else
				{
					glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level,
						0, 0, widthes_[level], widthes_[level],
						gl_format, gl_type, &tex_data_[face * num_mip_maps_ + level][0]);
				}
			}
			break;
		default:
			BOOST_ASSERT(false);
			break;
		}
	}
}